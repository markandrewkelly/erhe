#include "tools/physics_tool.hpp"

#include "editor_log.hpp"
#include "editor_rendering.hpp"
#include "editor_scenes.hpp"
#include "scene/node_physics.hpp"
#include "scene/scene_root.hpp"
#include "scene/viewport_window.hpp"
#include "scene/viewport_windows.hpp"
#include "tools/fly_camera_tool.hpp"
#include "tools/tools.hpp"
#include "windows/operations.hpp"

#include "erhe/application/commands/command_context.hpp"
#include "erhe/application/commands/commands.hpp"
#include "erhe/application/imgui/imgui_windows.hpp"
#include "erhe/application/renderers/line_renderer.hpp"
#include "erhe/application/view.hpp"
#include "erhe/physics/iconstraint.hpp"
#include "erhe/physics/iworld.hpp"
#include "erhe/scene/mesh.hpp"
#include "erhe/toolkit/profile.hpp"

#if defined(ERHE_GUI_LIBRARY_IMGUI)
#   include <imgui.h>
#endif

namespace editor
{

void Physics_tool_drag_command::try_ready(
    erhe::application::Command_context& context
)
{
    if (get_tool_state() != erhe::application::State::Inactive)
    {
        log_physics->info("PT state not inactive");
        return;
    }

    Scene_view* scene_view = reinterpret_cast<Scene_view*>(context.get_input_context());
    if (m_physics_tool.on_drag_ready(scene_view))
    {
        log_physics->info("PT set ready");
        set_ready(context);
    }
}

auto Physics_tool_drag_command::try_call(
    erhe::application::Command_context& context
) -> bool
{
    if (get_tool_state() == erhe::application::State::Inactive)
    {
        return false;
    }

    Scene_view* scene_view = reinterpret_cast<Scene_view*>(context.get_input_context());
    if (
        m_physics_tool.on_drag(scene_view) &&
        (get_tool_state() == erhe::application::State::Ready)
    )
    {
        set_active(context);
    }

    return get_tool_state() == erhe::application::State::Active;
}

void Physics_tool_drag_command::on_inactive(
    erhe::application::Command_context& context
)
{
    static_cast<void>(context);

    log_physics->info("PT on_inactive");
    if (get_tool_state() == erhe::application::State::Active)
    {
        m_physics_tool.release_target();
    }
}

Physics_tool::Physics_tool()
    : erhe::components::Component{c_type_name}
    , m_drag_command             {*this}
{
}

Physics_tool::~Physics_tool() noexcept
{
    if (m_target_constraint)
    {
        auto* world = physics_world();
        if (world != nullptr)
        {
            world->remove_constraint(m_target_constraint.get());
        }
    }
}

[[nodiscard]] auto Physics_tool::physics_world() const -> erhe::physics::IWorld*
{
    ERHE_VERIFY(m_editor_scenes);

    if (!m_target_mesh)
    {
        return nullptr;
    }

    auto* scene_root = reinterpret_cast<Scene_root*>(m_target_mesh->node_data.host);
    if (scene_root == nullptr)
    {
        return nullptr;
    }

    return &scene_root->physics_world();
}

void Physics_tool::declare_required_components()
{
    require<erhe::application::Commands>();
    require<Operations>();
    require<Tools     >();
}

void Physics_tool::initialize_component()
{
    const auto& tools = get<Tools>();
    tools->register_tool(this);
    tools->register_hover_tool(this);

    const auto commands = get<erhe::application::Commands>();
    commands->register_command(&m_drag_command);
    commands->bind_command_to_mouse_drag(&m_drag_command, erhe::toolkit::Mouse_button_right);
    commands->bind_command_to_controller_trigger(&m_drag_command, 0.5f, 0.45f, true);
    get<Operations>()->register_active_tool(this);

}

void Physics_tool::post_initialize()
{
    m_line_renderer_set = get<erhe::application::Line_renderer_set>();
    m_editor_scenes     = get<Editor_scenes>();
    m_fly_camera        = get<Fly_camera_tool>();
    m_viewport_windows  = get<Viewport_windows>();
}

auto Physics_tool::description() -> const char*
{
    return c_title.data();
}

[[nodiscard]] auto Physics_tool::get_mode() const -> Physics_tool_mode
{
    return m_mode;
}

void Physics_tool::set_mode(Physics_tool_mode value)
{
    m_mode = value;
}

auto Physics_tool::acquire_target(Scene_view* scene_view) -> bool
{
    log_physics->warn("acquire_target() ...");
    if (scene_view == nullptr)
    {
        log_physics->warn("Cant target: No scene_view");
        return false;
    }

    const auto& content = scene_view->get_hover(Hover_entry::content_slot);
    if (
        !content.valid ||
        !content.position.has_value()
    )
    {
        log_physics->warn("Cant target: No content");
        return false;
    }

    const auto p0_opt = scene_view->get_control_ray_origin_in_world();
    if (!p0_opt.has_value())
    {
        log_physics->warn("Cant target: No ray origin");
        return false;
    }

    const glm::vec3 view_position   = p0_opt.value();
    const glm::vec3 target_position = content.position.value();

    m_target_mesh             = content.mesh;
    m_target_distance         = glm::distance(view_position, target_position);
    m_target_position_in_mesh = m_target_mesh->transform_point_from_world_to_local(
        content.position.value()
    );
    m_target_node_physics     = get_physics_node(m_target_mesh.get());

    if (!m_target_node_physics)
    {
        log_physics->warn("Cant target: No physics mesh");
        m_target_mesh.reset();
        return false;
    }

    erhe::physics::IRigid_body* rigid_body = m_target_node_physics->rigid_body();
    if (rigid_body->get_motion_mode() == erhe::physics::Motion_mode::e_static)
    {
        log_physics->warn("Cant target: Static mesh");
        return false;
    }

    m_original_linear_damping  = rigid_body->get_linear_damping();
    m_original_angular_damping = rigid_body->get_angular_damping();

    rigid_body->set_damping(
        m_linear_damping,
        m_angular_damping
    );

    // TODO Make this happen automatically
    if (m_target_constraint)
    {
        auto* world = physics_world();
        if (world != nullptr)
        {
            world->remove_constraint(m_target_constraint.get());
        }
        /// TODO Should we also do this? m_target_constraint.reset();
    }

    log_physics->info("PT Target acquired - OK");

    return true;
}

void Physics_tool::release_target()
{
    log_physics->info("PT Target released");

    if (m_target_constraint)
    {
        if (m_target_node_physics)
        {
            erhe::physics::IRigid_body* rigid_body = m_target_node_physics->rigid_body();
            rigid_body->set_damping(
                m_original_linear_damping,
                m_original_angular_damping
            );
            rigid_body->end_move();
            m_target_node_physics.reset();
        }
        auto* world = physics_world();
        if (world != nullptr)
        {
            world->remove_constraint(m_target_constraint.get());
        }
        m_target_constraint.reset();
    }
    m_target_mesh.reset();
}

void Physics_tool::begin_point_to_point_constraint()
{
    log_physics->info("PT Begin point to point constraint");

    m_target_constraint = erhe::physics::IConstraint::create_point_to_point_constraint_unique(
        m_target_node_physics->rigid_body(),
        m_target_position_in_mesh
    );
    m_target_constraint->set_impulse_clamp(m_impulse_clamp);
    m_target_constraint->set_damping      (m_damping);
    m_target_constraint->set_tau          (m_tau);
    m_target_node_physics->rigid_body()->begin_move();
    auto* world = physics_world();
    if (world != nullptr)
    {
        world->add_constraint(m_target_constraint.get());
    }
}

auto Physics_tool::on_drag_ready(Scene_view* scene_view) -> bool
{
    if (!is_enabled())
    {
        log_physics->info("PT not enabled");
        return false;
    }
    if (!acquire_target(scene_view))
    {
        return false;
    }

    begin_point_to_point_constraint();

    log_physics->info("PT drag {} ready", m_target_mesh->name());
    return true;
}

#if 0
auto Physics_tool::on_force_ready() -> bool
{
    if (!is_enabled())
    {
        return false;
    }
    if (!acquire_target())
    {
        log_physics->info("Physics tool force - acquire target failed");
        return false;
    }

    begin_point_to_point_constraint();

    log_physics->info("Physics tool force {} ready", m_target_mesh->name());
    return true;
}
#endif

void Physics_tool::tool_hover(Scene_view* scene_view)
{
    if (scene_view == nullptr)
    {
        return;
    }

    const auto& hover = scene_view->get_hover(Hover_entry::content_slot);
    m_hover_mesh = hover.mesh;
}

auto Physics_tool::on_drag(Scene_view* scene_view) -> bool
{
    if (!is_enabled())
    {
        return false;
    }
    auto* world = physics_world();
    if (world == nullptr)
    {
        return false;
    }
    if (!world->is_physics_updates_enabled())
    {
        return false;
    }
    if (scene_view == nullptr)
    {
        return false;
    }

    const auto end = m_mode == Physics_tool_mode::Drag
        ? scene_view->get_control_position_in_world_at_distance(m_target_distance)
        : scene_view->get_control_ray_origin_in_world();
    if (!end.has_value())
    {
        return false;
    }

    if (!m_target_mesh)
    {
        return false;
    }

    if (m_mode == Physics_tool_mode::Drag)
    {
        m_target_position_start = glm::vec3{m_target_mesh->world_from_node() * glm::vec4{m_target_position_in_mesh, 1.0f}};
        m_target_position_end   = end.value();

        if (m_target_constraint)
        {
            m_target_constraint->set_pivot_in_b(m_target_position_end);
        }
    }
    else
    {
        m_target_position_start = glm::vec3{m_target_mesh->world_from_node() * glm::vec4{m_target_position_in_mesh, 1.0f}};

        float max_radius = 0.0f;
        for (const auto& primitive : m_target_mesh->mesh_data.primitives)
        {
            max_radius = std::max(
                max_radius,
                primitive.gl_primitive_geometry.bounding_sphere.radius
            );
        }
        m_target_mesh_size   = max_radius;
        m_to_end_direction   = glm::normalize(end.value() - m_target_position_start);
        m_to_start_direction = glm::normalize(m_target_position_start - end.value());
        const double distance = glm::distance(end.value(), m_target_position_start);
        if (distance > max_radius * 4.0)
        {
            m_target_position_end = m_target_position_start + m_force_distance * distance * m_to_end_direction;
        }
        else
        {
            m_target_position_end = end.value() + m_force_distance * distance * m_to_start_direction;
        }
        m_target_distance = distance;

        m_target_constraint->set_pivot_in_b(m_target_position_end);
    }

    return true;
}

void Physics_tool::tool_render(const Render_context& /*context*/)
{
    ERHE_PROFILE_FUNCTION

    if (!m_target_constraint)
    {
        return;
    }
    erhe::application::Line_renderer& line_renderer = m_line_renderer_set->hidden;
    line_renderer.set_line_color(0xffffffffu);
    line_renderer.set_thickness(4.0f);
    line_renderer.add_lines(
        {
            {
                m_target_position_start,
                m_target_position_end
            }
        }
    );
    m_line_renderer_set->hidden.set_line_color(0xffff0000u);
    m_line_renderer_set->hidden.add_lines(
        {
            {
                m_target_position_start,
                m_target_position_start + m_to_end_direction
            }
        }
    );
    m_line_renderer_set->hidden.set_line_color(0xff00ff00u);
    m_line_renderer_set->hidden.add_lines(
        {
            {
                m_target_position_start,
                m_target_position_start + m_to_start_direction
            }
        }
    );
}

void Physics_tool::tool_properties()
{
#if defined(ERHE_GUI_LIBRARY_IMGUI)
    //int command = m_active_command;
    ////const bool drag = ImGui::Button("Drag"); ImGui::SameLine();
    ////const bool push = ImGui::Button("Push"); ImGui::SameLine();
    ////const bool pull = ImGui::Button("Pull"); ImGui::SameLine();
    ////if (drag)
    ////{
    ////    m_mode = Physics_tool_mode::Drag;
    ////}
    ////if (push)
    ////{
    ////    m_mode = Physics_tool_mode::Push;
    ////}
    ////if (pull)
    ////{
    ////    m_mode = Physics_tool_mode::Pull;
    ////}

    ImGui::Text("State: %s",     erhe::application::c_state_str[static_cast<int>(m_drag_command.get_tool_state())]);
    if (m_hover_mesh)
    {
        ImGui::Text("Hover mesh: %s", m_hover_mesh->name().c_str());
    }
    ImGui::Text("Mesh: %s",            m_target_mesh ? m_target_mesh->name().c_str() : "");
    ImGui::Text("Drag distance: %f",   m_target_distance);
    ImGui::Text("Target distance: %f", m_target_distance);
    ImGui::Text("Target Size: %f",     m_target_mesh_size);
    ImGui::Text("Constraint: %s",      m_target_constraint ? "yes" : "no");
    const ImGuiSliderFlags logarithmic = ImGuiSliderFlags_Logarithmic;
    ImGui::SliderFloat("Force Distance",  &m_force_distance, -10.0f, 10.0f, "%.2f", logarithmic);
    ImGui::SliderFloat("Tau",             &m_tau,             0.0f,   0.1f);
    ImGui::SliderFloat("Damping",         &m_damping,         0.0f,   1.0f);
    ImGui::SliderFloat("Impulse Clamp",   &m_impulse_clamp,   0.0f, 100.0f);
    ImGui::SliderFloat("Linear Damping",  &m_linear_damping,  0.0f,   1.0f);
    ImGui::SliderFloat("Angular Damping", &m_angular_damping, 0.0f,   1.0f);
#endif
}

} // namespace editor
