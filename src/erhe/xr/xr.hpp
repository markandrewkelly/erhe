#pragma once

#include <openxr/openxr_reflection.h>

#include "erhe/gl/gl.hpp"
#include "erhe/gl/wrapper_enums.hpp"
#include "erhe/log/log.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdarg>

namespace erhe::xr {

class Pose
{
public:
    glm::quat orientation{};
    glm::vec3 position{};
};

class Render_view
{
public:
    uint32_t            slot;
    Pose                view_pose;
    float               fov_left;
    float               fov_right;
    float               fov_up;
    float               fov_down;
    uint32_t            color_texture;
    uint32_t            depth_texture;
    gl::Internal_format color_format;
    gl::Internal_format depth_format;
    uint32_t            width;
    uint32_t            height;
    bool                composition_alpha;
    float               near_depth;
    float               far_depth;
};

inline auto to_glm(const XrQuaternionf& q) -> glm::quat
{
    return glm::quat{q.w, q.x, q.y, q.z};
    //return glm::make_quat(&q.x);
}

inline auto to_glm(const XrVector3f& v) -> glm::vec3
{
    return glm::make_vec3(&v.x);
}

inline auto to_glm(const XrPosef& p) -> glm::mat4
{
    glm::mat4 orientation = glm::mat4_cast(to_glm(p.orientation));
    glm::mat4 translation = glm::translate(glm::mat4{1}, to_glm(p.position));
    return translation * orientation;
}

auto to_string_message_severity(XrDebugUtilsMessageSeverityFlagsEXT severity_flags) -> std::string;
auto to_string_message_type    (XrDebugUtilsMessageTypeFlagsEXT     type_flags)     -> std::string;

auto check(XrResult result, spdlog::level::level_enum log_level = spdlog::level::level_enum::err) -> bool;

#define ERHE_XR_CHECK(xr_result) if (!check(xr_result)) { return false; }

void check_gl_context_in_current_in_this_thread();

class Hand_tracking_joint
{
public:
    XrHandJointLocationEXT location;
    XrHandJointVelocityEXT velocity;
};

extern const char* c_str(::XrActionType            e) noexcept;
extern const char* c_str(::XrEnvironmentBlendMode  e) noexcept;
extern const char* c_str(::XrEyeVisibility         e) noexcept;
extern const char* c_str(::XrFormFactor            e) noexcept;
extern const char* c_str(::XrObjectType            e) noexcept;
extern const char* c_str(::XrResult                e) noexcept;
extern const char* c_str(::XrReferenceSpaceType    e) noexcept;
extern const char* c_str(::XrSessionState          e) noexcept;
extern const char* c_str(::XrStructureType         e) noexcept;
extern const char* c_str(::XrViewConfigurationType e) noexcept;
extern const char* c_str(::XrHandJointEXT          e) noexcept;

}
