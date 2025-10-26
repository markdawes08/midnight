#pragma once
#include <functional>
#include <vector>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

namespace gfx {

class GfxContext {
public:
  void init(GLFWwindow* window, bool enableValidation);
  void draw_frame(const std::function<void(VkCommandBuffer)>& record);
  void request_recreate() { recreateSwapchain_ = true; }
  void cleanup();

  VkExtent2D extent() const { return swapExtent_; }
  VkExtent2D swap_extent() const noexcept { return swapExtent_; }
  VkRenderPass render_pass() const { return renderPass_; }
  VkDevice device() const { return device_; }
  VkPhysicalDevice physical_device() const { return gpu_; }
  uint64_t swapchain_version() const { return swapVersion_; }

private:
  void create_instance_surface(bool enableValidation);
  void pick_device_and_create();
  void create_swapchain();
  void create_render_pass();
  void create_framebuffers();
  void create_commands();
  void create_sync();

  void destroy_swapchain_dependents();
  void recreate_swapchain();

  GLFWwindow* window_{};

  vkb::Instance vkbInst_{};
  VkInstance instance_{};
  VkSurfaceKHR surface_{};
  VkPhysicalDevice gpu_{};
  VkDevice device_{};
  uint32_t gfxQueueFamily_{};
  VkQueue gfxQueue_{};
  VkQueue presentQueue_{};

  vkb::Swapchain vkbSwap_{};
  VkSwapchainKHR swapchain_{};
  VkFormat swapFormat_{};
  VkExtent2D swapExtent_{};
  std::vector<VkImage> swapImages_{};
  std::vector<VkImageView> swapViews_{};

  VkRenderPass renderPass_{};
  std::vector<VkFramebuffer> framebuffers_{};
  VkCommandPool cmdPool_{};
  std::vector<VkCommandBuffer> cmdBufs_{};
  std::vector<VkFence> imagesInFlight_;


  struct FrameSync { VkSemaphore imgAvail{}, renderFin{}; VkFence inFlight{}; };
  static constexpr int kFramesInFlight = 2;
  FrameSync sync_[kFramesInFlight]{};
  size_t frameIndex_ = 0;

  bool recreateSwapchain_ = false;
  uint32_t lastFBW_ = 0, lastFBH_ = 0;
  uint64_t swapVersion_ = 1;
};

}