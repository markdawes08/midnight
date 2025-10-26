#include <volk.h>
#include "gfx/pipeline.hpp"
#include "gfx/context.hpp"
#include <stdexcept>
#include <vector>
#include <array>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace gfx {

static std::vector<char> read_file(const char* path){
  FILE* f = std::fopen(path, "rb"); if(!f) throw std::runtime_error(std::string("open ")+path);
  std::fseek(f, 0, SEEK_END); long sz = std::ftell(f); std::rewind(f);
  std::vector<char> buf(sz); if(sz>0) std::fread(buf.data(),1,(size_t)sz,f); std::fclose(f); return buf;
}

static VkShaderModule make_shader(VkDevice dev, const std::vector<char>& code){
  VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  ci.codeSize = code.size(); ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
  VkShaderModule m{}; if(vkCreateShaderModule(dev,&ci,nullptr,&m)!=VK_SUCCESS) throw std::runtime_error("shader module");
  return m;
}

void PlaneRenderer::init(GfxContext& ctx){
  device_ = ctx.device();
  phys_   = ctx.physical_device();

  // Create a huge XZ quad at y=0
  std::array<float, 12> verts = {
      -1000.f, 0.f, -1000.f,
       1000.f, 0.f, -1000.f,
      -1000.f, 0.f,  1000.f,
       1000.f, 0.f,  1000.f,
  };
  vbo_ = make_host_buffer(phys_, device_, sizeof(verts), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  upload_bytes(vbo_, verts.data(), sizeof(verts));

  create_pipeline(ctx);
}

void PlaneRenderer::cleanup(VkDevice device){
  destroy_pipeline();
  destroy_buffer(vbo_);
}

void PlaneRenderer::destroy_pipeline(){
  if(pipeline_) { vkDestroyPipeline(device_, pipeline_, nullptr); pipeline_ = VK_NULL_HANDLE; }
  if(layout_)   { vkDestroyPipelineLayout(device_, layout_, nullptr); layout_   = VK_NULL_HANDLE; }
}

void PlaneRenderer::ensure_pipeline(GfxContext& ctx){
  if(knownSwapVersion_ != ctx.swapchain_version()){
    destroy_pipeline();
    create_pipeline(ctx);
  }
}

void PlaneRenderer::create_pipeline(GfxContext& ctx){
  knownSwapVersion_ = ctx.swapchain_version();

  // Load shaders
  // Compiled by CMake into build/shaders/*.spv
  auto vs = read_file(SHADER_DIR "/plane.vert.spv");
  auto fs = read_file(SHADER_DIR "/solid_green.frag.spv");
  VkShaderModule vsm = make_shader(device_, vs);
  VkShaderModule fsm = make_shader(device_, fs);

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vsm;
  stages[0].pName  = "main";
  stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fsm;
  stages[1].pName  = "main";

  // Push constants: mat4 MVP in vertex stage
  VkPushConstantRange pcr{}; pcr.offset = 0; pcr.size = sizeof(glm::mat4); pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
  if(vkCreatePipelineLayout(device_, &plci, nullptr, &layout_) != VK_SUCCESS) throw std::runtime_error("pipeline layout");

  // Vertex input (position only)
  VkVertexInputBindingDescription bind{}; bind.binding = 0; bind.stride = sizeof(float)*3; bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  VkVertexInputAttributeDescription attr{}; attr.location = 0; attr.binding = 0; attr.format = VK_FORMAT_R32G32B32_SFLOAT; attr.offset = 0;
  VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vi.vertexBindingDescriptionCount = 1; vi.pVertexBindingDescriptions = &bind;
  vi.vertexAttributeDescriptionCount = 1; vi.pVertexAttributeDescriptions = &attr;

  VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; ia.primitiveRestartEnable = VK_FALSE;

  // Dynamic viewport/scissor so we don’t need to rebuild on size changes
  VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vp.viewportCount = 1; vp.scissorCount = 1;
  VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  VkDynamicState dynStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
  dyn.dynamicStateCount = 2; dyn.pDynamicStates = dynStates;

  VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState cba{};
  cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                     | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  cba.blendEnable = VK_FALSE;
  VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cb.attachmentCount = 1; cb.pAttachments = &cba;

  VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  gp.stageCount = 2; gp.pStages = stages;
  gp.pVertexInputState = &vi; gp.pInputAssemblyState = &ia; gp.pViewportState = &vp;
  gp.pRasterizationState = &rs; gp.pMultisampleState = &ms; gp.pColorBlendState = &cb; gp.pDynamicState = &dyn;
  gp.layout = layout_; gp.renderPass = ctx.render_pass(); gp.subpass = 0;

  if(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline_) != VK_SUCCESS)
    throw std::runtime_error("graphics pipeline");

  // We can destroy shader modules after pipeline creation
  vkDestroyShaderModule(device_, vsm, nullptr);
  vkDestroyShaderModule(device_, fsm, nullptr);
}

void PlaneRenderer::record(VkCommandBuffer cmd, GfxContext& ctx, const glm::mat4& mvp){
  ensure_pipeline(ctx);

  // Dynamic viewport/scissor setup
  VkExtent2D e = ctx.swap_extent();
  VkViewport vp{0.f, 0.f, (float)e.width, (float)e.height, 0.f, 1.f};
  VkRect2D   sc{{0,0}, e};
  vkCmdSetViewport(cmd, 0, 1, &vp);
  vkCmdSetScissor(cmd, 0, 1, &sc);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  vkCmdPushConstants(cmd, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);

  VkDeviceSize offs = 0; VkBuffer vb = vbo_.buf;
  vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offs);

  // Triangle strip over 4 verts: (0,1,2,3) forms two triangles covering the quad
  vkCmdDraw(cmd, 4, 1, 0, 0);
}

} // namespace gfx