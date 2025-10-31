#pragma once
#include <volk.h>
#include <glm/mat4x4.hpp>
#include "gfx/buffer.hpp"
#include "gfx/context.hpp"

namespace gfx {

class GfxContext;

class PlaneRenderer {
public:
  void init(GfxContext& ctx);
  void cleanup(VkDevice device);

  // Recreate pipeline if render pass changed (tracked via version)
  void ensure_pipeline(GfxContext& ctx);

  // Record draw commands (assumes render pass already begun)
  void record(VkCommandBuffer cmd, GfxContext& ctx, const glm::mat4& mvp);

private:
  VkDevice device_{};
  VkPhysicalDevice phys_{};
  VkPipelineLayout layout_{};
  VkPipeline pipeline_{};
  Buffer vbo_{}; // 4 verts for a big quad in XZ at y=0

  uint64_t knownSwapVersion_ = ~0ull; // invalid

  void create_pipeline(GfxContext& ctx);
  void destroy_pipeline();
};

class CubeRenderer {
public:
  void init(GfxContext& ctx);
  void cleanup(VkDevice /*device*/) { destroy_pipeline(); destroy_buffer(vbo_); }
  void ensure_pipeline(GfxContext& ctx) {
    if (knownSwapVersion_ != ctx.swapchain_version()) { destroy_pipeline(); create_pipeline(ctx); }
  }
  void record(VkCommandBuffer cmd, GfxContext& ctx, const glm::mat4& mvp);

private:
  VkDevice device_{};
  VkPhysicalDevice phys_{};
  VkPipelineLayout layout_{};
  VkPipeline pipeline_{};
  Buffer vbo_{};
  uint64_t knownSwapVersion_ = ~0ull;

  void create_pipeline(GfxContext& ctx);
  void destroy_pipeline(){
    if (pipeline_) { vkDestroyPipeline(device_, pipeline_, nullptr); pipeline_ = VK_NULL_HANDLE; }
    if (layout_)   { vkDestroyPipelineLayout(device_, layout_, nullptr); layout_ = VK_NULL_HANDLE; }
  }
};


}