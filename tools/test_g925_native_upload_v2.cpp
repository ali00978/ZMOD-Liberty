// G925 Native v2: exact Vulkan upload order/barriers with an allocation-free
// bounded batch queue. Link-free replay against the real Vulkan NDK headers.
#include <bit>
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>
#include "graphics/gta4_native/native_texture_upload_batch.h"

using namespace rex::graphics::gta4_native;

static VkImage Image(uint64_t n) {
  static_assert(sizeof(VkImage) == sizeof(uint64_t));
  return std::bit_cast<VkImage>(n);
}
static VkBuffer Buffer(uint64_t n) {
  static_assert(sizeof(VkBuffer) == sizeof(uint64_t));
  return std::bit_cast<VkBuffer>(n);
}

static void Replay(size_t count) {
  NativeTextureUploadBatch batch;
  assert(batch.empty());
  size_t barrier_calls = 0;
  size_t copied = 0;
  for (size_t i = 0; i < count; ++i) {
    NativeTextureUploadBatch::Upload upload{};
    upload.buffer = Buffer(4000 + i);
    upload.image = Image(8000 + i);
    upload.range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    upload.range.levelCount = 1;
    upload.range.layerCount = 1;
    VkBufferImageCopy copy{};
    copy.bufferOffset = i * 1024;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {16, 16, 1};
    upload.copies.push_back(copy);
    batch.Add(std::move(upload));
    assert(!batch.empty());
  }
  assert(batch.full() == (count == NativeTextureUploadBatch::kCapacity));
  const auto barriers = [&](VkPipelineStageFlags src, VkPipelineStageFlags dst,
                            std::span<const VkImageMemoryBarrier> barriers) {
    assert(barrier_calls < 2);
    assert(barriers.size() == count);
    if (barrier_calls == 0) {
      assert(src == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
      assert(dst == VK_PIPELINE_STAGE_TRANSFER_BIT);
    } else {
      assert(src == VK_PIPELINE_STAGE_TRANSFER_BIT);
      assert(dst == (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT));
    }
    for (size_t i = 0; i < count; ++i) {
      const auto& bar = barriers[i];
      assert(bar.sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
      assert(bar.image == Image(8000 + i));
      assert(bar.subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
      assert(bar.subresourceRange.levelCount == 1);
      assert(bar.subresourceRange.layerCount == 1);
      assert(bar.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
      assert(bar.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
      if (barrier_calls == 0) {
        assert(bar.dstAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT);
        assert(bar.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED);
        assert(bar.newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      } else {
        assert(bar.srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT);
        assert(bar.dstAccessMask == VK_ACCESS_SHADER_READ_BIT);
        assert(bar.oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        assert(bar.newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      }
    }
    ++barrier_calls;
  };
  const auto copies = [&](const NativeTextureUploadBatch::Upload& upload) {
    assert(copied < count);
    assert(upload.buffer == Buffer(4000 + copied));
    assert(upload.image == Image(8000 + copied));
    assert(upload.copies.size() == 1);
    assert(upload.copies[0].bufferOffset == copied * 1024);
    ++copied;
  };
  batch.Flush(barriers, copies);
  assert(copied == count);
  assert(barrier_calls == (count ? 2u : 0u));
  assert(batch.empty() && !batch.full());
  batch.Flush(barriers, copies);
  assert(copied == count && barrier_calls == (count ? 2u : 0u));
}

static void ReuseTheSameBatch() {
  NativeTextureUploadBatch batch;
  for (size_t pass = 0; pass < 1000; ++pass) {
    const size_t count = pass % NativeTextureUploadBatch::kCapacity + 1;
    for (size_t i = 0; i < count; ++i) {
      NativeTextureUploadBatch::Upload u{};
      u.buffer = Buffer(17 + i);
      u.image = Image(88 + i);
      u.range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      u.range.levelCount = 1;
      u.range.layerCount = 1;
      u.copies.resize(2);
      batch.Add(std::move(u));
    }
    size_t writes = 0, barriers = 0;
    batch.Flush([&](VkPipelineStageFlags, VkPipelineStageFlags,
                    std::span<const VkImageMemoryBarrier> b) {
        assert(b.size() == count);
        ++barriers;
      }, [&](const NativeTextureUploadBatch::Upload& u) {
        assert(u.copies.size() == 2);
        ++writes;
      });
    assert(batch.empty());
    assert(writes == count && barriers == 2);
  }
}

int main() {
  for (size_t count : {size_t(0), size_t(1), size_t(2), size_t(63),
                       size_t(127), size_t(128)}) {
    Replay(count);
  }
  ReuseTheSameBatch();
}
