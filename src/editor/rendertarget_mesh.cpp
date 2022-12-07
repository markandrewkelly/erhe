﻿// #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "rendertarget_mesh.hpp"
#include "editor_log.hpp"
#include "renderers/forward_renderer.hpp"
#include "renderers/mesh_memory.hpp"
#include "renderers/programs.hpp"
#include "renderers/render_context.hpp"
#include "scene/content_library.hpp"
#include "scene/scene_root.hpp"
#include "scene/viewport_window.hpp"
#include "windows/viewport_config.hpp"

#if defined(ERHE_XR_LIBRARY_OPENXR)
#   include "xr/hand_tracker.hpp"
#   include "xr/headset_view.hpp"
#endif

#include "erhe/application/configuration.hpp"
#include "erhe/application/view.hpp"
#include "erhe/gl/wrapper_functions.hpp"
#include "erhe/geometry/shapes/regular_polygon.hpp"
#include "erhe/graphics/buffer_transfer_queue.hpp"
#include "erhe/graphics/framebuffer.hpp"
#include "erhe/graphics/sampler.hpp"
#include "erhe/graphics/texture.hpp"
#include "erhe/graphics/opengl_state_tracker.hpp"
#include "erhe/primitive/primitive_builder.hpp"
#include "erhe/primitive/material.hpp"
#include "erhe/scene/mesh.hpp"
#include "erhe/scene/scene.hpp"
#include "erhe/toolkit/bit_helpers.hpp"
#include "erhe/toolkit/math_util.hpp"
#include "erhe/toolkit/profile.hpp"
#include "erhe/toolkit/window.hpp"

#if defined(ERHE_XR_LIBRARY_OPENXR)
#   include "erhe/xr/headset.hpp"
#endif

//#include <GLFW/glfw3.h> // TODO Fix dependency ?

namespace editor
{

Rendertarget_mesh::Rendertarget_mesh(
    Scene_root&                         host_scene_root,
    Viewport_window&                    host_viewport_window,
    const erhe::components::Components& components,
    const int                           width,
    const int                           height,
    const double                        pixels_per_meter
)
    : erhe::scene::Mesh     {"Rendertarget Node"}
    , m_host_scene_root     {host_scene_root}
    , m_host_viewport_window{host_viewport_window}
    //, m_pipeline_state_tracker{components.get<erhe::graphics::OpenGL_state_tracker>()}
    , m_viewport_config     {components.get<Viewport_config>()}
    , m_pixels_per_meter    {pixels_per_meter}
{
    enable_flag_bits(erhe::scene::Scene_item_flags::rendertarget);

    init_rendertarget(width, height);
    add_primitive(components);
}

auto Rendertarget_mesh::static_type_name() -> const char*
{
    return "Rendertarget_mesh";
}

auto Rendertarget_mesh::type_name() const -> const char*
{
    return "Rendertarget_mesh";
}

void Rendertarget_mesh::init_rendertarget(
    const int width,
    const int height
)
{
    using Texture     = erhe::graphics::Texture;
    using Framebuffer = erhe::graphics::Framebuffer;

    m_texture = std::make_shared<Texture>(
        Texture::Create_info{
            .target          = gl::Texture_target::texture_2d,
            .internal_format = gl::Internal_format::srgb8_alpha8,
            .use_mipmaps     = true,
            .sample_count    = 0,
            .width           = width,
            .height          = height
        }
    );
    m_texture->set_debug_label("Rendertarget Node");
    const float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.85f };
    gl::clear_tex_image(
        m_texture->gl_name(),
        0,
        gl::Pixel_format::rgba,
        gl::Pixel_type::float_,
        &clear_value[0]
    );

    m_sampler = std::make_shared<erhe::graphics::Sampler>(
        gl::Texture_min_filter::linear_mipmap_linear,
        gl::Texture_mag_filter::nearest,
        -0.666f
    );

    Framebuffer::Create_info create_info;
    create_info.attach(gl::Framebuffer_attachment::color_attachment0, m_texture.get());
    m_framebuffer = std::make_shared<Framebuffer>(create_info);
    m_framebuffer->set_debug_label("Rendertarget Node");
}

void Rendertarget_mesh::add_primitive(
    const erhe::components::Components& components
)
{
    auto& material_library = m_host_scene_root.content_library()->materials;
    auto& mesh_memory      = *components.get<Mesh_memory>().get();

    m_material = material_library.make(
        "Rendertarget Node",
        glm::vec4{0.1f, 0.1f, 0.2f, 1.0f}
    );
    m_material->texture = m_texture;
    m_material->sampler = m_sampler;

    m_local_width  = static_cast<double>(m_texture->width ()) / m_pixels_per_meter;
    m_local_height = static_cast<double>(m_texture->height()) / m_pixels_per_meter;

    auto geometry = erhe::geometry::shapes::make_rectangle(
        m_local_width,
        m_local_height,
        false,
        true
    );

    const auto shared_geometry = std::make_shared<erhe::geometry::Geometry>(
        std::move(geometry)
    );

    auto primitive = erhe::primitive::make_primitive(
        *shared_geometry.get(),
        mesh_memory.build_info
    );

    mesh_data.primitives.emplace_back(
        erhe::primitive::Primitive{
            .material              = m_material,
            .gl_primitive_geometry = primitive
        }
    );

    mesh_memory.gl_buffer_transfer_queue->flush();

    enable_flag_bits(
        erhe::scene::Scene_item_flags::visible |
        erhe::scene::Scene_item_flags::id      |
        erhe::scene::Scene_item_flags::rendertarget
    );

    //// m_node_raytrace = std::make_shared<Node_raytrace>(shared_geometry);

    //// add_to_raytrace_scene(m_host_scene_root.raytrace_scene(), m_node_raytrace);
    //// this->attach(m_node_raytrace);
}

auto Rendertarget_mesh::texture() const -> std::shared_ptr<erhe::graphics::Texture>
{
    return m_texture;
}

auto Rendertarget_mesh::framebuffer() const -> std::shared_ptr<erhe::graphics::Framebuffer>
{
    return m_framebuffer;
}

#if defined(ERHE_XR_LIBRARY_OPENXR)
void Rendertarget_mesh::update_headset(Headset_view& headset_view)
{
    const auto* headset = headset_view.get_headset();
    if (headset != nullptr)
    {
        m_controller_pose = headset->controller_pose();
        m_controller_trigger_value = headset->trigger_value()->currentState;
        //// if (m_controller_trigger_value > 0.5f)
        //// {
        ////     const auto controller_orientation = glm::mat4_cast(m_controller_pose.orientation);
        ////     const auto controller_direction   = glm::vec3{controller_orientation * glm::vec4{0.0f, 0.0f, -1.0f, 0.0f}};
        ////     const auto intersection = erhe::toolkit::intersect_plane<float>(
        ////         glm::vec3{this->direction_in_world()},
        ////         glm::vec3{this->position_in_world()},
        ////         m_controller_pose.position,
        ////         controller_direction
        ////     );
        ////     if (!intersection.has_value())
        ////     {
        ////         return;
        ////     }
        ////     const auto world_position      = m_controller_pose.position + intersection.value() * controller_direction;
        ////     const auto window_position_opt = world_to_window(world_position);
        ////     if (window_position_opt.has_value())
        ////     {
        ////         m_pointer = window_position_opt;
        ////         m_pointer_finger = Finger_point{
        ////             .finger       = static_cast<std::size_t>(XR_HAND_JOINT_INDEX_TIP_EXT),
        ////             .finger_point = m_controller_pose.position,
        ////             .point        = world_position
        ////         };
        ////     }
        ////     return;
        //// }
    }

    auto* hand_tracker = headset_view.get_hand_tracker();
    if (hand_tracker == nullptr)
    {
        return;
    }
    const auto thumb_opt  = hand_tracker->get_hand(Hand_name::Right).get_joint(XR_HAND_JOINT_THUMB_TIP_EXT);
    const auto index_opt  = hand_tracker->get_hand(Hand_name::Right).get_joint(XR_HAND_JOINT_INDEX_TIP_EXT);
    const auto middle_opt = hand_tracker->get_hand(Hand_name::Right).get_joint(XR_HAND_JOINT_MIDDLE_TIP_EXT);
    m_pointer_finger.reset();
    if (!index_opt.has_value())
    {
        return;
    }
    if (thumb_opt.has_value() && middle_opt.has_value())
    {
        const auto thumb    = thumb_opt .value();
        const auto middle   = middle_opt.value();
        const auto distance = glm::distance(thumb.position, middle.position);
        m_finger_trigger = distance < 0.014f;
    }

    auto const* node = get_node();
    if (node == nullptr)
    {
        return;
    }

    const auto pointer      = index_opt.value();
    const auto direction    = glm::vec3{pointer.orientation * glm::vec4{0.0f, 0.0f, 1.0f, 0.0f}};
    const auto intersection = erhe::toolkit::intersect_plane<float>(
        glm::vec3{node->direction_in_world()},
        glm::vec3{node->position_in_world()},
        pointer.position,
        direction
    );
    if (!intersection.has_value())
    {
        return;
    }
    const auto world_position      = pointer.position + intersection.value() * direction;
    const auto window_position_opt = world_to_window(world_position);
    if (window_position_opt.has_value())
    {
        m_pointer = window_position_opt;
        m_pointer_finger = Finger_point{
            .finger       = static_cast<std::size_t>(XR_HAND_JOINT_INDEX_TIP_EXT),
            .finger_point = pointer.position,
            .point        = world_position
        };
    }
}
#endif

auto Rendertarget_mesh::update_pointer() -> bool
{
    m_pointer.reset();

    const auto& rendertarget_hover = m_host_viewport_window.get_hover(Hover_entry::rendertarget_slot);
    if (!rendertarget_hover.valid)
    {
        return false;
    }
    const auto opt_origin_in_world    = m_host_viewport_window.get_control_ray_origin_in_world();
    const auto opt_direction_in_world = m_host_viewport_window.get_control_ray_direction_in_world();
    if (
        !opt_origin_in_world.has_value() ||
        !opt_direction_in_world.has_value()
    )
    {
        return false;
    }

    auto const* node = get_node();
    if (node == nullptr)
    {
        return false;
    }

    const glm::vec3 origin_position_in_world = opt_origin_in_world.value();
    const glm::vec3 direction_in_world       = opt_direction_in_world.value();
    const glm::vec3 origin_in_mesh           = node->transform_point_from_world_to_local(origin_position_in_world);
    const glm::vec3 direction_in_mesh        = node->transform_direction_from_world_to_local(direction_in_world      );

    const glm::vec3 origo      { 0.0f, 0.0f, 0.0f};
    const glm::vec3 unit_axis_z{ 0.0f, 0.0f, 1.0f};
    const auto hit = erhe::toolkit::intersect_plane<float>(
        unit_axis_z,
        origo,
        origin_in_mesh,
        direction_in_mesh
    );

    if (!hit.has_value())
    {
        return false;
    }

    {
        const glm::vec3 hit_position_in_mesh = origin_in_mesh + hit.value() * direction_in_mesh;
        const glm::vec2 a{
            hit_position_in_mesh.x / m_local_width,
            hit_position_in_mesh.y / m_local_height
        };
        const glm::vec2 b{
            a.x + 0.5f,
            0.5f - a.y
        };
        if (
            (b.x < 0.0f) ||
            (b.y < 0.0f) ||
            (b.x > 1.0f) ||
            (b.y > 1.0f)
        )
        {
            return false;
        }
        const glm::vec2 hit_position_in_viewport{
            m_texture->width() * (1.0f - b.x),
            m_texture->height() * b.y
        };

        SPDLOG_LOGGER_TRACE(
            log_pointer,
            "rt ray hit position {}",
            hit_position_in_viewport
        );

        m_pointer = hit_position_in_viewport;

        return true;
    }
}

[[nodiscard]] auto Rendertarget_mesh::world_to_window(
    glm::vec3 position_in_world
) const -> std::optional<glm::vec2>
{
    auto const* node = get_node();
    if (node == nullptr)
    {
        return {};
    }

    const glm::vec3 position_in_mesh = node->transform_point_from_world_to_local(position_in_world);
    const glm::vec2 a{
        position_in_mesh.x / m_local_width,
        position_in_mesh.y / m_local_height
    };
    const glm::vec2 b{
        a.x + 0.5f,
        0.5f - a.y
    };
    if (
        (b.x < 0.0f) ||
        (b.y < 0.0f) ||
        (b.x > 1.0f) ||
        (b.y > 1.0f)
    )
    {
        return {};
    }
    return glm::vec2{
        m_texture->width() * (1.0f - b.x),
        m_texture->height() * b.y
    };
}

void Rendertarget_mesh::bind()
{
    gl::bind_framebuffer(gl::Framebuffer_target::draw_framebuffer, m_framebuffer->gl_name());
    gl::viewport        (0, 0, m_texture->width(), m_texture->height());
}

void Rendertarget_mesh::clear(glm::vec4 clear_color)
{
    //m_pipeline_state_tracker->shader_stages.reset();
    //m_pipeline_state_tracker->color_blend.execute(
    //    erhe::graphics::Color_blend_state::color_blend_disabled
    //);

    // TODO Should probably restore m_pipeline_state_tracker code above instead
    gl::color_mask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    gl::clear_color(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    gl::clear      (gl::Clear_buffer_mask::color_buffer_bit);
}

void Rendertarget_mesh::render_done()
{
    gl::generate_texture_mipmap(m_texture->gl_name());

    if (m_viewport_config->rendertarget_mesh_lod_bias != m_sampler->lod_bias)
    {
        m_sampler = std::make_shared<erhe::graphics::Sampler>(
            gl::Texture_min_filter::linear_mipmap_linear,
            gl::Texture_mag_filter::nearest,
            m_viewport_config->rendertarget_mesh_lod_bias
        );
        m_material->sampler = m_sampler;
    }
}

[[nodiscard]] auto Rendertarget_mesh::get_pointer() const -> std::optional<glm::vec2>
{
    return m_pointer;
}

#if defined(ERHE_XR_LIBRARY_OPENXR)
[[nodiscard]] auto Rendertarget_mesh::get_pointer_finger() const -> std::optional<Finger_point>
{
    return m_pointer_finger;
}

[[nodiscard]] auto Rendertarget_mesh::get_finger_trigger() const -> bool
{
    return m_finger_trigger;
}

[[nodiscard]] auto Rendertarget_mesh::get_controller_pose() const -> const erhe::xr::Pose&
{
    return m_controller_pose;
}

[[nodiscard]] auto Rendertarget_mesh::get_controller_trigger() const -> float
{
    return m_controller_trigger_value;
}
#endif

[[nodiscard]] auto Rendertarget_mesh::width() const -> float
{
    return static_cast<float>(m_texture->width());
}

[[nodiscard]] auto Rendertarget_mesh::height() const -> float
{
    return static_cast<float>(m_texture->height());
}

[[nodiscard]] auto Rendertarget_mesh::pixels_per_meter() const -> double
{
    return m_pixels_per_meter;
}

auto is_rendertarget(const erhe::scene::Scene_item* const scene_item) -> bool
{
    if (scene_item == nullptr)
    {
        return false;
    }
    using namespace erhe::toolkit;
    return test_all_rhs_bits_set(scene_item->get_flag_bits(), erhe::scene::Scene_item_flags::rendertarget);
}

auto is_rendertarget(const std::shared_ptr<erhe::scene::Scene_item>& scene_item) -> bool
{
    return is_rendertarget(scene_item.get());
}

auto as_rendertarget(erhe::scene::Scene_item* const scene_item) -> Rendertarget_mesh*
{
    if (scene_item == nullptr)
    {
        return nullptr;
    }
    using namespace erhe::toolkit;
    if (!test_all_rhs_bits_set(scene_item->get_flag_bits(), erhe::scene::Scene_item_flags::rendertarget))
    {
        return nullptr;
    }
    return reinterpret_cast<Rendertarget_mesh*>(scene_item);
}

auto as_rendertarget(const std::shared_ptr<erhe::scene::Scene_item>& scene_item) -> std::shared_ptr<Rendertarget_mesh>
{
    if (!scene_item)
    {
        return {};
    }
    using namespace erhe::toolkit;
    if (!test_all_rhs_bits_set(scene_item->get_flag_bits(), erhe::scene::Scene_item_flags::rendertarget))
    {
        return {};
    }
    return std::static_pointer_cast<Rendertarget_mesh>(scene_item);
}

auto get_rendertarget(
    const erhe::scene::Node* const node
) -> std::shared_ptr<Rendertarget_mesh>
{
    for (const auto& attachment : node->attachments())
    {
        auto rendertarget = as_rendertarget(attachment);
        if (rendertarget)
        {
            return rendertarget;
        }
    }
    return {};
}

}  // namespace editor