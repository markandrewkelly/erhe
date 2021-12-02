#include "windows/brushes.hpp"
#include "log.hpp"
#include "tools.hpp"
#include "operations/operation_stack.hpp"
#include "operations/insert_operation.hpp"
#include "renderers/line_renderer.hpp"
#include "scene/brush.hpp"
#include "scene/helpers.hpp"
#include "scene/node_physics.hpp"
#include "scene/scene_root.hpp"
#include "tools/grid_tool.hpp"
#include "tools/pointer_context.hpp"
#include "windows/materials.hpp"

#include "erhe/geometry/operation/clone.hpp"
#include "erhe/geometry/geometry.hpp"
#include "erhe/imgui/imgui_helpers.hpp"
#include "erhe/primitive/material.hpp"
#include "erhe/primitive/primitive_builder.hpp"
#include "erhe/scene/mesh.hpp"
#include "erhe/scene/scene.hpp"

#include <glm/gtx/transform.hpp>
#include <imgui.h>

namespace editor
{

using namespace std;
using namespace glm;
using namespace erhe::geometry;
using erhe::geometry::Polygon; // Resolve conflict with wingdi.h BOOL Polygon(HDC,const POINT *,int)
using namespace erhe::primitive;
using namespace erhe::scene;
using namespace erhe::toolkit;

Brush_create_context::Brush_create_context(
    erhe::primitive::Build_info_set& build_info_set,
    erhe::primitive::Normal_style    normal_style
)
    : build_info_set{build_info_set}
    , normal_style  {normal_style}
{
}

Brushes::Brushes()
    : erhe::components::Component{c_name}
    , Imgui_window               {c_title}
{
}

Brushes::~Brushes() = default;

auto Brushes::state() const -> State
{
    return m_state;
}

void Brushes::connect()
{
    m_grid_tool       = get<Grid_tool>();
    m_materials       = get<Materials>();
    m_operation_stack = get<Operation_stack>();
    m_scene_root      = require<Scene_root>();
    m_selection_tool  = get<Selection_tool>();
}

void Brushes::initialize_component()
{
    m_selected_brush_index = 0;

    get<Editor_tools>()->register_tool(this);
}

auto Brushes::allocate_brush(Build_info_set& build_info_set)
-> std::shared_ptr<Brush>
{
    std::lock_guard<std::mutex> lock(m_brush_mutex);
    const auto brush = std::make_shared<Brush>(build_info_set);
    m_brushes.push_back(brush);
    return brush;
}

auto Brushes::make_brush(
    erhe::geometry::Geometry&&                         geometry,
    const Brush_create_context&                        context,
    const shared_ptr<erhe::physics::ICollision_shape>& collision_shape
) -> std::shared_ptr<Brush>
{
    ERHE_PROFILE_FUNCTION

    const auto shared_geometry = make_shared<erhe::geometry::Geometry>(move(geometry));
    return make_brush(shared_geometry, context, collision_shape);
}

auto Brushes::make_brush(
    shared_ptr<erhe::geometry::Geometry>               geometry,
    const Brush_create_context&                        context,
    const shared_ptr<erhe::physics::ICollision_shape>& collision_shape
) -> std::shared_ptr<Brush>
{
    ERHE_PROFILE_FUNCTION

    geometry->build_edges();
    geometry->compute_polygon_normals();
    geometry->compute_tangents();
    geometry->compute_polygon_centroids();
    geometry->compute_point_normals(c_point_normals_smooth);

    const float density = 1.0f;

    const auto mass_properties = geometry->get_mass_properties();

    Brush::Create_info create_info{
        geometry,
        context.build_info_set,
        context.normal_style,
        density,
        mass_properties.volume,
        collision_shape
    };

    const auto brush = allocate_brush(context.build_info_set);
    brush->initialize(create_info);
    return brush;
}

auto Brushes::make_brush(
    shared_ptr<erhe::geometry::Geometry> geometry,
    const Brush_create_context&          context,
    Collision_volume_calculator          collision_volume_calculator,
    Collision_shape_generator            collision_shape_generator
)
-> std::shared_ptr<Brush>
{
    ERHE_PROFILE_FUNCTION

    geometry->build_edges();
    geometry->compute_polygon_normals();
    geometry->compute_tangents();
    geometry->compute_polygon_centroids();
    geometry->compute_point_normals(c_point_normals_smooth);
    const Brush::Create_info create_info{
        geometry,
        context.build_info_set,
        context.normal_style,
        1.0f, // density
        collision_volume_calculator,
        collision_shape_generator
    };

    const auto brush = allocate_brush(context.build_info_set);
    brush->initialize(create_info);
    return brush;
}

void Brushes::cancel_ready()
{
    m_state = State::Passive;
    if (m_brush_mesh)
    {
        remove_brush_mesh();
    }
}

void Brushes::remove_brush_mesh()
{
    if (m_brush_mesh)
    {
        log_brush.trace("removing brush mesh\n");
        remove_from_scene_layer(m_scene_root->scene(), *m_scene_root->brush_layer().get(), m_brush_mesh);
        m_brush_mesh->unparent();
        m_brush_mesh.reset();
    }
}

auto Brushes::update(Pointer_context& pointer_context) -> bool
{
    ERHE_PROFILE_FUNCTION

    if (pointer_context.priority_action != Action::add)
    {
        remove_brush_mesh();
        return false;
    }

    m_hover_content     = pointer_context.hover_content;
    m_hover_tool        = pointer_context.hover_tool;
    m_hover_mesh        = pointer_context.hover_mesh;
    m_hover_primitive   = pointer_context.hover_primitive;
    m_hover_local_index = pointer_context.hover_local_index;
    m_hover_geometry    = pointer_context.geometry;

    m_hover_position = (m_hover_content && pointer_context.hover_valid)
        ? pointer_context.position_in_world() 
        : optional<vec3>{};

    m_hover_normal = (m_hover_content && pointer_context.hover_valid)
        ? pointer_context.hover_normal
        : optional<vec3>{};

    if (m_hover_mesh && m_hover_position.has_value())
    {
        m_hover_position = m_hover_mesh->node_from_world() * vec4{m_hover_position.value(), 1.0f};
    }

    if ((m_state == State::Passive) &&
        pointer_context.mouse_button[Mouse_button_left].pressed &&
        m_brush_mesh)
    {
        m_state = State::Ready;
        return true;
    }

    if (m_state != State::Ready)
    {
        return false;
    }

    if (pointer_context.mouse_button[Mouse_button_left].released && m_brush_mesh)
    {
        do_insert_operation();
        remove_brush_mesh();
        m_state = State::Passive;
        return true;
    }

    return false;
}

void Brushes::render(const Render_context&)
{
}

void Brushes::render_update(const Render_context& render_context)
{
    if (render_context.pointer_context->priority_action != Action::add)
    {
        remove_brush_mesh();
        return;
    }

    update_mesh();
}

// Returns transform which places brush in parent (hover) mesh space.
auto Brushes::get_brush_transform() -> mat4
{
    if ((m_hover_mesh == nullptr) || (m_hover_geometry == nullptr) || (m_brush == nullptr))
    {
        return mat4{1};
    }

    const Polygon_id polygon_id = static_cast<const Polygon_id>(m_hover_local_index);
    const Polygon&   polygon    = m_hover_geometry->polygons[polygon_id];
    Reference_frame  hover_frame(*m_hover_geometry, polygon_id);
    Reference_frame  brush_frame = m_brush->get_reference_frame(polygon.corner_count);
    hover_frame.N *= -1.0f;
    hover_frame.B *= -1.0f;

    VERIFY(brush_frame.scale() != 0.0f);

    float scale = hover_frame.scale() / brush_frame.scale();

    m_transform_scale = scale;
    if (scale != 1.0f)
    {
        mat4 scale_transform = erhe::toolkit::create_scale(scale);
        brush_frame.transform_by(scale_transform);
    }

    debug_info.hover_frame_scale = hover_frame.scale();
    debug_info.brush_frame_scale = brush_frame.scale();
    debug_info.transform_scale   = scale;

    if (!m_snap_to_hover_polygon && m_hover_position.has_value())
    {
        hover_frame.centroid = m_hover_position.value();
        if (m_snap_to_grid)
        {
            hover_frame.centroid = m_grid_tool->snap(hover_frame.centroid);
        }
    }

    const mat4 hover_transform = hover_frame.transform();
    const mat4 brush_transform = brush_frame.transform();
    const mat4 inverse_brush   = inverse(brush_transform);
    const mat4 align           = hover_transform * inverse_brush;

    return align;
}

void Brushes::update_mesh_node_transform()
{
    if (!m_hover_position.has_value())
    {
        return;
    }

    VERIFY(m_brush_mesh);
    VERIFY(m_hover_mesh);

    const auto  transform    = get_brush_transform();
    const auto& brush_scaled = m_brush->get_scaled(m_transform_scale);
    if (m_brush_mesh->parent() != m_hover_mesh.get())
    {
        if (m_brush_mesh->parent())
        {
            log_brush.trace(
                "m_brush_mesh->parent() = {} ({})\n",
                m_brush_mesh->parent()->name(),
                m_brush_mesh->parent()->node_type()
            );
        }
        else
        {
            log_brush.trace("m_brush_mesh->parent() = (none)\n");
        }

        if (m_hover_mesh)
        {
            log_brush.trace(
                "m_hover_mesh = {} ({})\n",
                m_hover_mesh->name(),
                m_hover_mesh->node_type()
            );
        }
        else
        {
            log_brush.trace("m_hover_mesh = (none)\n");
        }

        if (m_hover_mesh)
        {
            m_hover_mesh->attach(m_brush_mesh);
        }
    }
    m_brush_mesh->set_parent_from_node(transform);
    m_brush_mesh->data.primitives.front().primitive_geometry = brush_scaled.primitive_geometry;
}

void Brushes::do_insert_operation()
{
    if (!m_hover_position.has_value() || (m_brush == nullptr))
    {
        return;
    }

    log_brush.trace("{} scale = {}\n", __func__, m_transform_scale);

    const auto hover_from_brush = get_brush_transform();
    const auto world_from_brush = m_hover_mesh->world_from_node() * hover_from_brush;
    const auto material         = m_materials->selected_material();
    const auto instance         = m_brush->make_instance(
        world_from_brush,
        material,
        m_transform_scale
    );
    instance.mesh->visibility_mask() &= ~Node::c_visibility_brush;
    instance.mesh->visibility_mask() |=
        (Node::c_visibility_content     |
         Node::c_visibility_shadow_cast |
         Node::c_visibility_id);

    const Mesh_insert_remove_operation::Context context{
        m_selection_tool,
        m_scene_root->scene(),
        m_scene_root->content_layer(),
        m_scene_root->physics_world(),
        instance.mesh,
        instance.node_physics,
        m_hover_mesh,
        Scene_item_operation::Mode::insert
    };
    auto op = make_shared<Mesh_insert_remove_operation>(context);
    m_operation_stack->push(op);
}

void Brushes::add_brush_mesh()
{
    if ((m_brush == nullptr) || !m_hover_position.has_value())
    {
        return;
    }

    const auto material = m_materials->selected_material();
    if (!material)
    {
        return;
    }

    const auto& brush_scaled = m_brush->get_scaled(m_transform_scale);
    m_brush_mesh = m_scene_root->make_mesh_node(
        brush_scaled.primitive_geometry->source_geometry->name,
        brush_scaled.primitive_geometry,
        material,
        *m_scene_root->brush_layer().get()
    );
    m_brush_mesh->visibility_mask() &= ~(Node::c_visibility_id);
    m_brush_mesh->visibility_mask() |= Node::c_visibility_brush;
    update_mesh_node_transform();
}

void Brushes::update_mesh()
{
    if (!m_brush_mesh)
    {
        add_brush_mesh();
        return;
    }

    if ((m_brush == nullptr) || !m_hover_position.has_value())
    {
        remove_brush_mesh();
    }

    update_mesh_node_transform();
}

void Brushes::tool_properties()
{
    using namespace erhe::imgui;

    ImGui::InputFloat("Hover scale",     &debug_info.hover_frame_scale);
    ImGui::InputFloat("Brush scale",     &debug_info.brush_frame_scale);
    ImGui::InputFloat("Transform scale", &debug_info.transform_scale);
    ImGui::SliderFloat("Scale", &m_scale, 0.0f, 2.0f);
    make_check_box("Snap to Polygon", &m_snap_to_hover_polygon);
    make_check_box(
        "Snap to Grid",
        &m_snap_to_grid,
        m_snap_to_hover_polygon
            ? Item_mode::disabled
            : Item_mode::normal
    );
}

void Brushes::imgui(Pointer_context&)
{
    using namespace erhe::imgui;

    ImGui::Begin("Brushes");

    const size_t brush_count = m_brushes.size();

    {
        auto button_size = ImVec2(ImGui::GetContentRegionAvailWidth(), 0.0f);
        for (int i = 0; i < static_cast<int>(brush_count); ++i)
        {
            auto* brush = m_brushes[i].get();
            bool button_pressed = make_button(
                brush->geometry->name.c_str(),
                (m_selected_brush_index == i)
                    ? Item_mode::active
                    : Item_mode::normal,
                button_size
            );
            if (button_pressed)
            {
                m_selected_brush_index = i;
                m_brush = brush;
            }
        }
    }
    ImGui::End();
}

}
