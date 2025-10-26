#include <stdexcept>
#include <array>
#include <spdlog/spdlog.h>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <volk.h>
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include "gfx/context.hpp"

namespace gfx {

static VkClearValue kClearBlue() { VkClearValue v{}; v.color = {{0.53f, 0.81f, 0.98f, 1.0f}}; return v; }

void GfxContext::init(GLFWwindow* window, bool enableValidation) {
  window_ = window;
  if (volkInitialize() != VK_SUCCESS) throw std::runtime_error("volkInitialize failed");
  create_instance_surface(enableValidation);
  pick_device_and_create();
  create_swapchain();
  create_render_pass();
  create_framebuffers();
  create_commands();
  create_sync();
}

void GfxContext::create_instance_surface(bool enableValidation) {
  uint32_t extCount = 0; const char** exts = glfwGetRequiredInstanceExtensions(&extCount);
  std::vector<const char*> extList(exts, exts + extCount);

  vkb::InstanceBuilder b;
  for (auto e : extList) b.enable_extension(e);
  auto inst = b.set_app_name("midnight")
              .set_engine_name("midnight")
              .set_app_version(0, 1, 0)
              .set_engine_version(0, 1, 0)
              .request_validation_layers(enableValidation)
              .use_default_debug_messenger()
              .require_api_version(1, 3, 0)
              .build();
  if (!inst) throw std::runtime_error(inst.error().message());
  vkbInst_ = inst.value();
  instance_ = vkbInst_.instance;
  volkLoadInstance(instance_);

  if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
    throw std::runtime_error("glfwCreateWindowSurface failed");
}

void GfxContext::pick_device_and_create() {
  vkb::PhysicalDeviceSelector sel{vkbInst_, surface_};
  auto phys = sel.set_minimum_version(1, 3).select();
  if (!phys) throw std::runtime_error(phys.error().message());
  vkb::PhysicalDevice vkbPhys = phys.value();
  gpu_ = vkbPhys.physical_device;

  vkb::DeviceBuilder db{vkbPhys};
  auto dev = db.build();
  if (!dev) throw std::runtime_error(dev.error().message());
  vkb::Device vkbDev = dev.value();
  device_ = vkbDev.device;
  volkLoadDevice(device_);

  gfxQueue_       = vkbDev.get_queue(vkb::QueueType::graphics).value();
  presentQueue_   = vkbDev.get_queue(vkb::QueueType::present).value();
  gfxQueueFamily_ = vkbDev.get_queue_index(vkb::QueueType::graphics).value();
}

void GfxContext::create_swapchain() {
  vkb::SwapchainBuilder sb{gpu_, device_, surface_};
  sb.use_default_format_selection()
    .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    .set_desired_min_image_count(2)
    .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR);

  auto sc = sb.build();
  if (!sc) {
    sc = vkb::SwapchainBuilder{gpu_, device_, surface_}
           .use_default_format_selection()
           .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
           .set_desired_min_image_count(2)
           .build();
  }
  if (!sc) throw std::runtime_error(sc.error().message());

  vkbSwap_    = sc.value();
  swapchain_  = vkbSwap_.swapchain;
  swapFormat_ = vkbSwap_.image_format;
  swapExtent_ = vkbSwap_.extent;
  swapImages_ = vkbSwap_.get_images().value();
  swapViews_  = vkbSwap_.get_image_views().value();
  imagesInFlight_.assign(swapImages_.size(), VK_NULL_HANDLE);

  lastFBW_ = swapExtent_.width;
  lastFBH_ = swapExtent_.height;
}

void GfxContext::create_render_pass() {
  VkAttachmentDescription color{};
  color.format = swapFormat_;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  VkSubpassDescription sub{};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &colorRef;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL; dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dep.srcAccessMask = 0;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  ci.attachmentCount = 1; ci.pAttachments = &color;
  ci.subpassCount = 1; ci.pSubpasses = &sub;
  ci.dependencyCount = 1; ci.pDependencies = &dep;

  if (vkCreateRenderPass(device_, &ci, nullptr, &renderPass_) != VK_SUCCESS)
    throw std::runtime_error("vkCreateRenderPass failed");
}

void GfxContext::create_framebuffers() {
  framebuffers_.resize(swapViews_.size());
  for (size_t i = 0; i < swapViews_.size(); ++i) {
    VkImageView atts[]{ swapViews_[i] };
    VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    ci.renderPass = renderPass_; ci.attachmentCount = 1; ci.pAttachments = atts;
    ci.width = swapExtent_.width; ci.height = swapExtent_.height; ci.layers = 1;
    if (vkCreateFramebuffer(device_, &ci, nullptr, &framebuffers_[i]) != VK_SUCCESS)
      throw std::runtime_error("vkCreateFramebuffer failed");
  }
}

void GfxContext::create_commands() {
  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.queueFamilyIndex = gfxQueueFamily_;
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  if (vkCreateCommandPool(device_, &pci, nullptr, &cmdPool_) != VK_SUCCESS)
    throw std::runtime_error("vkCreateCommandPool failed");

  cmdBufs_.resize(framebuffers_.size());
  VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool = cmdPool_;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = (uint32_t)cmdBufs_.size();
  if (vkAllocateCommandBuffers(device_, &ai, cmdBufs_.data()) != VK_SUCCESS)
    throw std::runtime_error("vkAllocateCommandBuffers failed");
}

void GfxContext::create_sync() {
  VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (int i = 0; i < kFramesInFlight; ++i) {
    if (vkCreateSemaphore(device_, &sci, nullptr, &sync_[i].imgAvail) != VK_SUCCESS ||
        vkCreateSemaphore(device_, &sci, nullptr, &sync_[i].renderFin) != VK_SUCCESS ||
        vkCreateFence(device_, &fci, nullptr, &sync_[i].inFlight) != VK_SUCCESS)
      throw std::runtime_error("sync creation failed");
  }
}

void GfxContext::destroy_swapchain_dependents() {
  if (!cmdBufs_.empty()) vkFreeCommandBuffers(device_, cmdPool_, (uint32_t)cmdBufs_.size(), cmdBufs_.data());
  cmdBufs_.clear();
  if (cmdPool_) { vkDestroyCommandPool(device_, cmdPool_, nullptr); cmdPool_ = VK_NULL_HANDLE; }
  for (auto fb : framebuffers_) if (fb) vkDestroyFramebuffer(device_, fb, nullptr);
  framebuffers_.clear();
  if (renderPass_) { vkDestroyRenderPass(device_, renderPass_, nullptr); renderPass_ = VK_NULL_HANDLE; }
  for (auto v : swapViews_) if (v) vkDestroyImageView(device_, v, nullptr);
  swapViews_.clear(); swapImages_.clear();
  imagesInFlight_.clear();
  if (swapchain_) { vkDestroySwapchainKHR(device_, swapchain_, nullptr); swapchain_ = VK_NULL_HANDLE; }
}

void GfxContext::recreate_swapchain() {
  int w = 0, h = 0;
  do {
    glfwGetFramebufferSize(window_, &w, &h);
    if (w == 0 || h == 0) {
      glfwWaitEvents();
    }
  } while (w == 0 || h == 0);

  vkDeviceWaitIdle(device_);

  destroy_swapchain_dependents();
  create_swapchain();
  create_render_pass();
  create_framebuffers();
  create_commands();

  ++swapVersion_;
  recreateSwapchain_ = false;
}


void GfxContext::draw_frame(const std::function<void(VkCommandBuffer)>& record) {
  int fbw = 0, fbh = 0;
  glfwGetFramebufferSize(window_, &fbw, &fbh);

  if (fbw == 0 || fbh == 0) {
    glfwWaitEventsTimeout(0.016);
    return;
  }

  if ((uint32_t)fbw != swapExtent_.width || (uint32_t)fbh != swapExtent_.height) {
    recreateSwapchain_ = true;
  }

  if (recreateSwapchain_) {
    recreate_swapchain();
  }

  auto& fs = sync_[frameIndex_ % kFramesInFlight];
  vkWaitForFences(device_, 1, &fs.inFlight, VK_TRUE, UINT64_MAX);

  uint32_t imageIndex = 0;
  VkResult acq = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, fs.imgAvail, VK_NULL_HANDLE, &imageIndex);

  if (acq == VK_ERROR_SURFACE_LOST_KHR) {
    vkDeviceWaitIdle(device_);
    if (surface_) { vkDestroySurfaceKHR(instance_, surface_, nullptr); surface_ = VK_NULL_HANDLE; }
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
      throw std::runtime_error("glfwCreateWindowSurface (recreate) failed");
    recreateSwapchain_ = true;
    return;
  }

  if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapchain_ = true;
    return;
  }

  bool suboptimal = (acq == VK_SUBOPTIMAL_KHR);
  if (acq != VK_SUCCESS && !suboptimal) {
    throw std::runtime_error("AcquireNextImage failed");
  }

  if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX);
  }

  imagesInFlight_[imageIndex] = sync_[frameIndex_ % kFramesInFlight].inFlight;

  vkResetFences(device_, 1, &sync_[frameIndex_ % kFramesInFlight].inFlight);
  vkResetCommandBuffer(cmdBufs_[imageIndex], 0);

  VkCommandBuffer cmd = cmdBufs_[imageIndex];
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  vkBeginCommandBuffer(cmd, &bi);

  VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  rp.renderPass = renderPass_;
  rp.framebuffer = framebuffers_[imageIndex];
  rp.renderArea.offset = {0, 0};
  rp.renderArea.extent = swapExtent_;
  VkClearValue clear = kClearBlue();
  rp.clearValueCount = 1;
  rp.pClearValues = &clear;
  vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

  if (record) record(cmd);

  vkCmdEndRenderPass(cmd);
  vkEndCommandBuffer(cmd);

  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.waitSemaphoreCount = 1; si.pWaitSemaphores = &fs.imgAvail; si.pWaitDstStageMask = &waitStage;
  si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
  si.signalSemaphoreCount = 1; si.pSignalSemaphores = &fs.renderFin;
  if (vkQueueSubmit(gfxQueue_, 1, &si, fs.inFlight) != VK_SUCCESS) throw std::runtime_error("QueueSubmit failed");

  VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &fs.renderFin;
  pi.swapchainCount = 1; pi.pSwapchains = &swapchain_; pi.pImageIndices = &imageIndex;
  VkResult pres = vkQueuePresentKHR(presentQueue_, &pi);
  if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
    recreateSwapchain_ = true;
  } else if (pres == VK_ERROR_SURFACE_LOST_KHR) {
    vkDeviceWaitIdle(device_);
    if (surface_) { vkDestroySurfaceKHR(instance_, surface_, nullptr); surface_ = VK_NULL_HANDLE; }
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
      throw std::runtime_error("glfwCreateWindowSurface (present path) failed");
    recreateSwapchain_ = true;
  } else if (pres != VK_SUCCESS) {
    throw std::runtime_error("QueuePresent failed");
  }
  ++frameIndex_;
}

void GfxContext::cleanup() {
  if (device_) vkDeviceWaitIdle(device_);
  for (int i = 0; i < kFramesInFlight; ++i) {
    if (sync_[i].imgAvail) vkDestroySemaphore(device_, sync_[i].imgAvail, nullptr);
    if (sync_[i].renderFin) vkDestroySemaphore(device_, sync_[i].renderFin, nullptr);
    if (sync_[i].inFlight)  vkDestroyFence(device_, sync_[i].inFlight, nullptr);
  }
  destroy_swapchain_dependents();
  if (vkbInst_.debug_messenger) {
    auto pfnDestroyDbg = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
    if (pfnDestroyDbg) {
      pfnDestroyDbg(instance_, vkbInst_.debug_messenger, nullptr);
    }
    vkbInst_.debug_messenger = VK_NULL_HANDLE;
  }
  if (device_)   { vkDestroyDevice(device_, nullptr); device_ = VK_NULL_HANDLE; }
  if (surface_)  { vkDestroySurfaceKHR(instance_, surface_, nullptr); surface_ = VK_NULL_HANDLE; }
  if (instance_) { vkDestroyInstance(instance_, nullptr); instance_ = VK_NULL_HANDLE; }
}

}
