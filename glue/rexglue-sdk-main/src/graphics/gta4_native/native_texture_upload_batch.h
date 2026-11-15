#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace rex::graphics::gta4_native {

// Records uploads to distinct, newly initialized images. Image and staging
// allocation ownership remains with the renderer's submission retirement path.
// Consumers must flush before reading any queued image. No image is published
// here, and queued handles must never be destroyed before Flush.
class NativeTextureUploadBatch {
 public:
  static constexpr size_t kCapacity = 128;
  struct Upload {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkImageSubresourceRange range{};
    std::vector<VkBufferImageCopy> copies;
  };

  // The old std::vector dynamically grew while the game prepared many
  // texture uploads for a frame. Fixed 128 slots avoid heap growth and copies
  // in the CPU-critical recording path; the exact Vulkan barrier and copy
  // sequence is unchanged.
  bool full() const { return count_ == kCapacity; }
  bool empty() const { return count_ == 0; }
  void Add(Upload upload) {
    assert(!full() && upload.image && upload.buffer && !upload.copies.empty());
    uploads_[count_++].emplace(std::move(upload));
  }

  template <typename EmitBarriers, typename EmitCopy>
  void Flush(EmitBarriers&& emit_barriers, EmitCopy&& emit_copy) {
    if (empty()) return;
    std::array<VkImageMemoryBarrier, kCapacity> barriers;
    for (size_t i = 0; i < count_; ++i) {
      auto& barrier = barriers[i];
      barrier = {};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = uploads_[i]->image;
      barrier.subresourceRange = uploads_[i]->range;
    }
    const std::span<const VkImageMemoryBarrier> span(barriers.data(), count_);
    emit_barriers(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, span);
    for (size_t i = 0; i < count_; ++i) emit_copy(*uploads_[i]);
    for (size_t i = 0; i < count_; ++i) {
      auto& barrier = barriers[i];
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    emit_barriers(VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                  span);
    // Release source copy lists without discarding the fixed slot capacity.
    // Neither the batch nor these CPU vectors own Vulkan resources.
    for (size_t i = 0; i < count_; ++i) uploads_[i].reset();
    count_ = 0;
  }

 private:
  // Disengaged slots construct no Upload vectors. Only active commands get
  // materialized, avoiding 128 unused vector constructions per batch.
  std::array<std::optional<Upload>, kCapacity> uploads_{};
  size_t count_ = 0;
};

}  // namespace rex::graphics::gta4_native
