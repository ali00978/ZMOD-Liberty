// Included in vulkan_presenter.cpp, inside rex::ui::vulkan. The producer
// owns this object; the paint thread sees only immutable GeneratedFrame leases.
#include "g925_mobile_framegen_spv.h"

struct VulkanPresenter::MobileFrameGen {
  static constexpr size_t kMaxPairs = 8;
  struct Pair {
    const VulkanDevice* device;
    VkExtent2D extent{};
    std::shared_ptr<GuestOutputImage> real, generated, source;
    std::shared_ptr<Pair> previous;
    VkCommandPool commands = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkDescriptorPool descriptors = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    bool pending = false;
    explicit Pair(const VulkanDevice* d) : device(d) {}
    ~Pair() {
      const auto& fn = device->functions(); const auto vk = device->device();
      if (pending) fn.vkWaitForFences(vk, 1, &fence, VK_TRUE, UINT64_MAX);
      if (commands) fn.vkDestroyCommandPool(vk, commands, nullptr);
      if (descriptors) fn.vkDestroyDescriptorPool(vk, descriptors, nullptr);
      if (fence) fn.vkDestroyFence(vk, fence, nullptr);
    }
    bool Init(VkExtent2D size, VkDescriptorSetLayout layout) {
      extent = size;
      const auto& fn = device->functions(); const auto vk = device->device();
      real = GuestOutputImage::Create(device, size.width, size.height);
      generated = GuestOutputImage::Create(device, size.width, size.height);
      if (!real || !generated) return false;
      VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      pool.queueFamilyIndex = device->queue_family_graphics_compute();
      pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
      if (fn.vkCreateCommandPool(vk, &pool, nullptr, &commands) != VK_SUCCESS) return false;
      VkCommandBufferAllocateInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
      cb.commandPool = commands; cb.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cb.commandBufferCount = 1;
      if (fn.vkAllocateCommandBuffers(vk, &cb, &command) != VK_SUCCESS) return false;
      VkFenceCreateInfo f{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
      if (fn.vkCreateFence(vk, &f, nullptr, &fence) != VK_SUCCESS) return false;
      VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
                                     {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};
      VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
      dp.maxSets = 1; dp.poolSizeCount = 2; dp.pPoolSizes = sizes;
      if (fn.vkCreateDescriptorPool(vk, &dp, nullptr, &descriptors) != VK_SUCCESS) return false;
      VkDescriptorSetAllocateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
      ds.descriptorPool = descriptors; ds.descriptorSetCount = 1; ds.pSetLayouts = &layout;
      return fn.vkAllocateDescriptorSets(vk, &ds, &set) == VK_SUCCESS;
    }
    void Collect() {
      if (!pending) return;
      if (device->functions().vkGetFenceStatus(device->device(), fence) == VK_SUCCESS) {
        pending = false; source.reset(); previous.reset();
      }
    }
  };

  explicit MobileFrameGen(const VulkanDevice* d) : device(d) {}
  ~MobileFrameGen() {
    // GPU submissions on this queue must retire before destroying pipelines.
    for (auto& pair : pool) {
      if (pair->pending) {
        device->functions().vkWaitForFences(device->device(), 1, &pair->fence, VK_TRUE, UINT64_MAX);
        pair->pending = false;
      }
    }
    for (auto& pair : pool) { pair->source.reset(); pair->previous.reset(); }
    history.reset(); pool.clear();
    const auto& fn = device->functions(); const auto vk = device->device();
    if (pipeline) fn.vkDestroyPipeline(vk, pipeline, nullptr);
    if (pipeline_layout) fn.vkDestroyPipelineLayout(vk, pipeline_layout, nullptr);
    if (layout) fn.vkDestroyDescriptorSetLayout(vk, layout, nullptr);
    if (sampler) fn.vkDestroySampler(vk, sampler, nullptr);
  }
  void ResetHistory() {
    history.reset(); last_frame = 0; last_ns = 0;
    for (auto& pair : pool) pair->Collect();
  }
  bool Init() {
    if (failed) return false;
    if (pipeline) return true;
    failed = true;
    if (!device->properties().shaderStorageImageExtendedFormats) return false;
    VkFormatProperties format{};
    device->vulkan_instance()->functions().vkGetPhysicalDeviceFormatProperties(
        device->physical_device(), kGuestOutputFormat, &format);
    constexpr VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if ((format.optimalTilingFeatures & needed) != needed) return false;
    const auto& fn = device->functions(); const auto vk = device->device();
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = 0.0f;
    if (fn.vkCreateSampler(vk, &si, nullptr, &sampler) != VK_SUCCESS) return false;
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
    VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dl.bindingCount = 3; dl.pBindings = bindings;
    if (fn.vkCreateDescriptorSetLayout(vk, &dl, nullptr, &layout) != VK_SUCCESS) return false;
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1; pl.pSetLayouts = &layout;
    if (fn.vkCreatePipelineLayout(vk, &pl, nullptr, &pipeline_layout) != VK_SUCCESS) return false;
    VkShaderModule shader = util::CreateShaderModule(device, kG925MobileFramegenSpv,
                                                       sizeof(kG925MobileFramegenSpv));
    if (!shader) return false;
    VkComputePipelineCreateInfo pc{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pc.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    pc.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; pc.stage.module = shader; pc.stage.pName = "main";
    pc.layout = pipeline_layout;
    const auto result = fn.vkCreateComputePipelines(vk, VK_NULL_HANDLE, 1, &pc, nullptr, &pipeline);
    fn.vkDestroyShaderModule(vk, shader, nullptr);
    if (result != VK_SUCCESS) return false;
    failed = false;
    REXLOG_INFO("G925 mobile FG: portable Vulkan compute initialized");
    return true;
  }
  std::shared_ptr<Pair> Acquire(VkExtent2D size) {
    for (auto& pair : pool) pair->Collect();
    for (auto& pair : pool) {
      if (pair->pending || pair.use_count() != 1) continue;
      if (pair->extent.width == size.width && pair->extent.height == size.height) return pair;
      auto replacement = std::make_shared<Pair>(device);
      if (!replacement->Init(size, layout)) return {};
      pair = replacement; return pair;
    }
    if (pool.size() >= kMaxPairs) return {};
    auto pair = std::make_shared<Pair>(device);
    if (!pair->Init(size, layout)) return {};
    pool.push_back(pair); return pair;
  }
  void Barrier(VkCommandBuffer cb, VkImage image, VkImageLayout old_layout,
               VkImageLayout new_layout, VkAccessFlags src, VkAccessFlags dst,
               VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask=src; b.dstAccessMask=dst; b.oldLayout=old_layout; b.newLayout=new_layout;
    b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.image=image; b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    device->functions().vkCmdPipelineBarrier(cb,src_stage,dst_stage,0,0,nullptr,0,nullptr,1,&b);
  }
  std::shared_ptr<GeneratedFrame> Process(const std::shared_ptr<GuestOutputImage>& input,
                                          uint32_t sequence, uint64_t epoch) {
    const auto size = input->extent();
    // Bound VRAM and work on mobile. Higher resolutions use the real frame.
    if (!size.width || !size.height || size.height > 900 ||
        uint64_t(size.width)*size.height > 1'500'000 || !Init()) {
      ResetHistory(); return {};
    }
    const uint64_t now = FramePacerNowNs();
    // Slow real rendering or a full pool should not cause extra copies every
    // scene. Briefly back off, then seed clean history for another attempt.
    if (now < cooldown_until_ns) { ResetHistory(); return {}; }
    if (history && epoch == last_epoch && sequence == last_frame) return {};
    const uint64_t delta = now > last_ns ? now-last_ns : 0;
    if (history && epoch == last_epoch && delta > 55'000'000) {
      cooldown_until_ns = now + 250'000'000;
      ResetHistory(); return {};
    }
    const bool interpolate = history && epoch == last_epoch && sequence == last_frame+1 &&
        history->extent.width == size.width && history->extent.height == size.height &&
        delta >= 15'000'000 && delta <= 55'000'000;
    auto pair = Acquire(size);
    if (!pair) { cooldown_until_ns = now + 250'000'000; ResetHistory(); return {}; }
    const auto& fn = device->functions(); const auto vk = device->device();
    auto access = input->AcquireAccess();
    if (!access.ticket().valid()) { ResetHistory(); return {}; }
    if (fn.vkResetCommandPool(vk, pair->commands, 0) != VK_SUCCESS ||
        fn.vkResetFences(vk, 1, &pair->fence) != VK_SUCCESS) { ResetHistory(); return {}; }
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    const auto cb=pair->command;
    if(fn.vkBeginCommandBuffer(cb,&begin)!=VK_SUCCESS) { ResetHistory(); return {}; }
    constexpr auto read=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    constexpr auto shaders=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    Barrier(cb,input->image(),read,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
    Barrier(cb,pair->real->image(),VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkImageCopy copy{};
    copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.extent={size.width,size.height,1};
    fn.vkCmdCopyImage(cb,input->image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        pair->real->image(),VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    Barrier(cb,input->image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,read,
        VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,shaders);
    Barrier(cb,pair->real->image(),VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,read,
        VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,shaders);
    if (interpolate) {
      Barrier(cb,pair->generated->image(),VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,
          0,VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      VkDescriptorImageInfo infos[] = {{sampler,history->real->view(),read},
          {sampler,pair->real->view(),read},{VK_NULL_HANDLE,pair->generated->view(),VK_IMAGE_LAYOUT_GENERAL}};
      VkWriteDescriptorSet writes[3]{};
      for(uint32_t i=0;i<3;++i) {
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[i].dstSet=pair->set;
        writes[i].dstBinding=i; writes[i].descriptorCount=1; writes[i].pImageInfo=&infos[i];
        writes[i].descriptorType=i==2?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      }
      fn.vkUpdateDescriptorSets(vk,3,writes,0,nullptr);
      fn.vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
      fn.vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,0,1,&pair->set,0,nullptr);
      fn.vkCmdDispatch(cb,(size.width+15)/16,(size.height+15)/16,1);
      Barrier(cb,pair->generated->image(),VK_IMAGE_LAYOUT_GENERAL,read,
          VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,shaders);
    }
    if(fn.vkEndCommandBuffer(cb)!=VK_SUCCESS) { ResetHistory(); return {}; }
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&cb;
    ImageAccessSubmit timeline;
    if (!timeline.Attach(submit,access.ticket())) { ResetHistory(); return {}; }
    {
      const auto queue=device->AcquireQueue(device->queue_family_graphics_compute(),0);
      if(fn.vkQueueSubmit(queue.queue(),1,&submit,pair->fence)!=VK_SUCCESS) {
        ResetHistory(); failed=true; return {};
      }
      access.Commit();
    }
    pair->pending=true; pair->source=input;
    if(interpolate) pair->previous=history;
    history=pair; last_frame=sequence; last_epoch=epoch; last_ns=now;
    if(!interpolate) return {};
    auto result=std::make_shared<GeneratedFrame>();
    result->generated=pair->generated->image(); result->generated_view=pair->generated->view();
    result->real=pair->real->image(); result->real_view=pair->real->view(); result->extent=size;
    result->lease=pair; result->epoch=epoch; result->interval_ns=std::max(uint64_t(16'666'667),delta/2);
    if (!result->valid(input->image(),size)) return {};
    if(!logged_pair) { logged_pair=true; REXLOG_INFO("G925 mobile FG: first interpolation pair submitted"); }
    return result;
  }

  const VulkanDevice* device;
  VkSampler sampler=VK_NULL_HANDLE;
  VkDescriptorSetLayout layout=VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout=VK_NULL_HANDLE;
  VkPipeline pipeline=VK_NULL_HANDLE;
  bool failed=false, logged_pair=false;
  std::vector<std::shared_ptr<Pair>> pool;
  std::shared_ptr<Pair> history;
  uint64_t last_epoch=0,last_ns=0,cooldown_until_ns=0;
  uint32_t last_frame=0;
};
