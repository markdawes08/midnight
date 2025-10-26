#include <stdexcept>
#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "gfx/context.hpp"
#include "gfx/pipeline.hpp"

static const char* kAppName = "midnight";
static const uint32_t kWidth = 1280;
static const uint32_t kHeight = 720;

static void glfw_error_callback(int code, const char* desc) { spdlog::error("GLFW error {}: {}", code, desc); }

int main(){
  try{
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::info("Starting {} (I2)", kAppName);

    glfwSetErrorCallback(glfw_error_callback);
    if(!glfwInit()) throw std::runtime_error("glfwInit failed");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow((int)kWidth,(int)kHeight,kAppName,nullptr,nullptr);
    if(!window) throw std::runtime_error("glfwCreateWindow failed");

    gfx::GfxContext ctx; ctx.init(window, /*validation*/true);

    gfx::PlaneRenderer plane; plane.init(ctx);

    glfwSetWindowUserPointer(window, &ctx);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w,int,int){
      auto* c = static_cast<gfx::GfxContext*>(glfwGetWindowUserPointer(w)); if(c) c->request_recreate();
    });

    while(!glfwWindowShouldClose(window)){
      glfwPollEvents();

      // Fixed camera for I2
      float aspect = (float)ctx.extent().width / (float)ctx.extent().height;
      glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);
      // GLM produces clip space with Y down when using Vulkan; flip Y to match Vulkan NDC
      proj[1][1] *= -1.0f;
      glm::mat4 view = glm::lookAt(glm::vec3(0.f, 3.f, 6.f), glm::vec3(0.f,0.f,0.f), glm::vec3(0.f,1.f,0.f));
      glm::mat4 model = glm::mat4(1.0f);
      glm::mat4 mvp = proj * view * model;

      ctx.draw_frame([&](VkCommandBuffer cmd){
        plane.record(cmd, ctx, mvp);
      });
    }

    // Force GPU to finish all work before destroying resources
    vkDeviceWaitIdle(ctx.device());

    plane.cleanup(ctx.device());
    ctx.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
  } catch(const std::exception& e){
    spdlog::critical("Fatal: {}", e.what());
    return 1;
  }
}