#include <stdexcept>
#include <cmath>
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

// --- Simple orbit camera ------------------------------------------------------
struct OrbitCamera {
  // angles in radians; yaw about +Y, pitch about +X
  float yaw   = glm::radians(0.0f);
  float pitch = glm::radians(25.0f);
  float radius = 8.0f;                 // distance from target
  glm::vec3 target{0.f, 0.f, 0.f};
  // Ground constraints
  float ground_y = 0.0f;
  float min_eye_h = 0.5f;

  // First-person style view: eye stays fixed; yaw/pitch only change the look direction.
  glm::mat4 view() const {
    const glm::vec3 up(0.f, 1.f, 0.f);
    glm::vec3 eye = target;
    if (eye.y < ground_y + min_eye_h) {
      eye.y = ground_y + min_eye_h;
    }
    glm::vec3 center = eye + forward_dir();
    return glm::lookAt(eye, center, up);
  }


  // Camera-space basis from yaw/pitch
  glm::vec3 forward_dir() const {
    float cp = std::cos(pitch), sp = std::sin(pitch);
    float cy = std::cos(yaw),   sy = std::sin(yaw);
    // +Z is forward when yaw=0 to match your eye computation
    return glm::normalize(glm::vec3(sy * cp, sp, cy * cp));
  }
  glm::vec3 right_dir() const {
    return glm::normalize(glm::cross(forward_dir(), glm::vec3(0.f, 1.f, 0.f)));
  }

  // Move the orbit *target* in camera-local axes (free-fly)
  void move_freefly(float right, float forward) {
    target += right_dir() * right + forward_dir() * forward;
  }

  // Pan the orbit target on the ground plane (Y=0) using camera-local right/forward.
  void pan_local(float dx, float dz) {
    // Right/forward from yaw, ignoring pitch to keep motion planar
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const glm::vec3 right{ cy, 0.f, -sy };
    const glm::vec3 fwd  { sy, 0.f,  cy };
    target += right * dx + fwd * dz;
    // Keep the target on the ground plane (optional, remove if you want free Y)
    target.y = 0.f;
  }

  // Clamp pitch so eye never goes below ground regardless of zoom radius.
  void constrain_to_ground() {
    const float kPitchMin = glm::radians(-89.0f);
    const float kPitchMax = glm::radians( 89.0f);
    pitch = glm::clamp(pitch, kPitchMin, kPitchMax);
    if (target.y < ground_y + min_eye_h) {
      target.y = ground_y + min_eye_h;
    }
  }
};

// AppState so window user pointer can route callbacks
struct AppState {
  gfx::GfxContext* ctx{};
  OrbitCamera cam{};
  bool dragging = false;
  double lastX = 0.0, lastY = 0.0;
};

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
    gfx::CubeRenderer  cube;  cube.init(ctx);

    AppState app{ &ctx };
    double lastTime = glfwGetTime();

    // Callbacks
    glfwSetWindowUserPointer(window, &app);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w,int, int){
      auto* a = static_cast<AppState*>(glfwGetWindowUserPointer(w));
      if(a && a->ctx) a->ctx->request_recreate();
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int){
      auto* a = static_cast<AppState*>(glfwGetWindowUserPointer(w));
      if(!a) return;
      if(button == GLFW_MOUSE_BUTTON_RIGHT){
        if(action == GLFW_PRESS){
          a->dragging = true;
          glfwGetCursorPos(w, &a->lastX, &a->lastY);
        }else if(action == GLFW_RELEASE){
          a->dragging = false;
        }
      }
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y){
      auto* a = static_cast<AppState*>(glfwGetWindowUserPointer(w));
      if(!a || !a->dragging) { return; }
      double dx = x - a->lastX;
      double dy = y - a->lastY;
      a->lastX = x; a->lastY = y;

      // Tunables
      const float rotSpeed = 0.005f;

      a->cam.yaw   -= (float)dx * rotSpeed;
      a->cam.pitch -= (float)dy * rotSpeed;
      // Keep camera above the ground while rotating
      a->cam.constrain_to_ground();
    });

    glfwSetScrollCallback(window, [](GLFWwindow* w, double /*xoff*/, double yoff){
      auto* a = static_cast<AppState*>(glfwGetWindowUserPointer(w));
      if(!a) return;
      // Exponential zoom feels nicer than linear
      const float zoomStep = 1.1f; // wheel notch scales by ~10%
      if(yoff > 0)      a->cam.radius /= zoomStep;
      else if(yoff < 0) a->cam.radius *= zoomStep;
      a->cam.radius = glm::clamp(a->cam.radius, 0.5f, 500.0f);
      a->cam.constrain_to_ground();
    });

    app.cam.constrain_to_ground();
    while(!glfwWindowShouldClose(window)){
      glfwPollEvents();

      // --- WASD camera panning ---
      double now = glfwGetTime();
      float dt = static_cast<float>(now - lastTime);
      lastTime = now;

      float speed = 30.0f;                 // units per second
      if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) speed *= 3.0f;

      float dx = 0.f, dz = 0.f;
      if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) dz += speed * dt; // forward
      if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) dz -= speed * dt; // back
      if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) dx += speed * dt; // right
      if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) dx -= speed * dt; // left

      if (dx != 0.f || dz != 0.f) {
        app.cam.move_freefly(dx, dz);
      }


      float aspect = (float)ctx.extent().width / (float)ctx.extent().height;
      glm::mat4 view = app.cam.view();
      glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 2000.0f);
      proj[1][1] *= -1.0f;

      glm::mat4 modelPlane = glm::mat4(1.0f);
      glm::mat4 modelCube  = glm::translate(glm::mat4(1.0f), glm::vec3(0.f, 5.f, 0.f)); // 5 units up

      glm::mat4 mvpPlane = proj * view * modelPlane;
      glm::mat4 mvpCube  = proj * view * modelCube;

      ctx.draw_frame([&](VkCommandBuffer cmd){
        plane.record(cmd, ctx, mvpPlane);
        cube.record(cmd,  ctx, mvpCube);
      });
    }

    // Ensure GPU is idle before destroying resources that might still be in use
    vkDeviceWaitIdle(ctx.device());
    cube.cleanup(ctx.device());
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
