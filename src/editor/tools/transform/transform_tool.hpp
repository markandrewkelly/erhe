#pragma once

#include "editor_message.hpp"
#include "operations/node_operation.hpp"
#include "tools/transform/handle_visualizations.hpp"
#include "tools/selection_tool.hpp"
#include "tools/tool.hpp"

#include "erhe/application/commands/command.hpp"
#include "erhe/application/imgui/imgui_window.hpp"
#include "erhe/components/components.hpp"
#include "erhe/message_bus/message_bus.hpp"
#include "erhe/physics/imotion_state.hpp"
#include "erhe/primitive/primitive_geometry.hpp"
#include "erhe/scene/node.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string_view>

namespace erhe::physics
{
    enum class Motion_mode : unsigned int;
}

namespace erhe::scene
{
    class Mesh;
    class Node;
}

namespace editor
{

class Compound_operation;
class Node_physics;
class Scene_root;
class Viewport_window;

class Transform_tool_drag_command
    : public erhe::application::Command
{
public:
    Transform_tool_drag_command();
    void try_ready  () override;
    auto try_call   () -> bool override;
    void on_inactive() override;
};

class Transform_tool
    : public erhe::application::Imgui_window
    , public erhe::components::Component
    , public Tool
{
public:
    class Config
    {
    public:
        float scale         {4.0f};
        bool  show_translate{true};
        bool  show_rotate   {false};
    };
    Config config;

    enum class Reference_mode : unsigned int
    {
        local = 0,
        parent,
        world,
    };

    static constexpr std::array<std::string_view, 3> c_reference_mode_strings =
    {
        "Local",
        "Parent",
        "World"
    };

    static constexpr int              c_priority {1};
    static constexpr std::string_view c_type_name{"Transform_tool"};
    static constexpr std::string_view c_title    {"Transform"};
    static constexpr uint32_t         c_type_hash{
        compiletime_xxhash::xxh32(
            c_type_name.data(),
            c_type_name.size(),
            {}
        )
    };

    Transform_tool ();
    ~Transform_tool() noexcept override;

    // Implements Component
    [[nodiscard]] auto get_type_hash() const -> uint32_t override { return c_type_hash; }
    void declare_required_components() override;
    void initialize_component       () override;
    void deinitialize_component     () override;

    // Implements Tool
    void tool_render(const Render_context& context) override;

    // Implements Imgui_window
    void imgui() override;

    // Public API
    void viewport_toolbar(bool& hovered);
    void set_translate   (bool enabled);
    void set_rotate      (bool enabled);
    void set_scale       (bool enabled);
    [[nodiscard]] auto is_trs_active() const -> bool;

    // Commands
    auto on_drag_ready() -> bool;
    auto on_drag      () -> bool;
    void end_drag     ();

    // For Handle_visualizations
    [[nodiscard]] auto is_x_translate_active  () const -> bool;
    [[nodiscard]] auto is_y_translate_active  () const -> bool;
    [[nodiscard]] auto is_z_translate_active  () const -> bool;
    [[nodiscard]] auto is_rotate_active       () const -> bool;
    [[nodiscard]] auto is_x_scale_active      () const -> bool;
    [[nodiscard]] auto is_y_scale_active      () const -> bool;
    [[nodiscard]] auto is_z_scale_active      () const -> bool;
    [[nodiscard]] auto get_active_handle      () const -> Handle;
    [[nodiscard]] auto get_hover_handle       () const -> Handle;
    [[nodiscard]] auto get_handle             (erhe::scene::Mesh* mesh) const -> Handle;
    [[nodiscard]] auto get_handle_type        (const Handle handle) const -> Handle_type;
    [[nodiscard]] auto get_tool_scene_root    () -> std::shared_ptr<Scene_root>;

private:
    void on_message         (Editor_message& message);
    void update_for_view    (Scene_view* scene_view);
    void update_target_nodes();
    void tool_hover         ();
    void touch              ();
    void begin_move         ();
    void end_move           ();
    void render_rotate      (const Render_context& context);

    class Debug_rendering
    {
    public:
        glm::vec3 m_P0{};
        glm::vec3 m_P1{};
        glm::vec3 m_Q0{};
        glm::vec3 m_Q1{};
        glm::vec3 m_R0{};
        glm::vec3 m_R1{};
        glm::vec2 m_ss_closest{};
        uint32_t  m_debug_color{0x00000000u};
        float     m_v_dot_n{0.0f};
        glm::vec3 m_pw{};
        glm::vec3 m_q0{};
        glm::vec3 m_q{};
    };

    class Drag
    {
    public:
        glm::mat4 initial_world_from_anchor{1.0f};
        glm::mat4 initial_anchor_from_world{1.0f};
        glm::vec3 initial_position_in_world{0.0f, 0.0f, 0.0f};
        float     initial_distance         {0.0f};
    };

    class Rotation_context
    {
    public:
        glm::vec3                normal              {0.0f}; // also rotation axis
        glm::vec3                reference_direction {0.0f};
        glm::vec3                center_of_rotation  {0.0f};
        std::optional<glm::vec3> intersection        {};
        float                    start_rotation_angle{0.0f};
        float                    current_angle       {0.0f};
        erhe::scene::Transform   world_from_anchor;
    };

    class Anchor_state
    {
    public:
        glm::vec3 pivot_point_in_world{0.0f, 0.0f, 0.0f};
        glm::mat4 anchor_translation  {1.0f};
        glm::quat anchor_rotation     {1.0f, 0.0f, 0.0f, 0.0f};
        glm::mat4 world_from_anchor   {1.0f};
    };

    [[nodiscard]] auto project_pointer_to_plane(Scene_view* scene_view, const glm::vec3 n, const glm::vec3 p) -> std::optional<glm::vec3>;
    [[nodiscard]] auto snap_translate          (const glm::vec3 translation) const -> glm::vec3;
    [[nodiscard]] auto snap_rotate             (const float angle_radians) const -> float;
    [[nodiscard]] auto offset_plane_origo      (const Handle handle, const glm::vec3 p) const -> glm::vec3;
    [[nodiscard]] auto project_to_offset_plane (const Handle handle, const glm::vec3 p, const glm::vec3 q) const -> glm::vec3;
    [[nodiscard]] auto get_axis_direction      () const -> glm::vec3;
    [[nodiscard]] auto get_plane_normal        (const bool world) const -> glm::vec3;
    [[nodiscard]] auto get_plane_side          (const bool world) const -> glm::vec3;
    [[nodiscard]] auto get_axis_color          (Handle handle) const -> glm::vec4;

    void set_local                         (bool local);

    void update_axis_translate_2d          (Viewport_window* viewport_window);
    void update_axis_translate_3d          (Scene_view* scene_view);
    void update_axis_translate_final       (const glm::vec3 drag_position);
    void update_plane_translate_2d         (Viewport_window* viewport_window);
    void update_plane_translate_3d         (Scene_view* scene_view);

    void update_rotate                     (Scene_view* scene_view);
    auto update_rotate_circle_around       (Scene_view* scene_view) -> bool;
    auto update_rotate_parallel            (Scene_view* scene_view) -> bool;
    void update_rotate_final               ();

    void update_axis_scale_2d              (Viewport_window* viewport_window);
    void update_axis_scale_3d              (Scene_view* scene_view);
    void update_plane_scale_2d             (Viewport_window* viewport_window);
    void update_plane_scale_3d             (Scene_view* scene_view);
    void update_scale_final                (const glm::vec3 drag_position);

    void update_world_from_anchor_transform(const glm::mat4& state);
    void update_transforms                 ();
    void update_visibility                 ();

    Transform_tool_drag_command               m_drag_command;
    erhe::application::Redirect_command       m_drag_redirect_update_command;
    erhe::application::Drag_enable_command    m_drag_enable_command;

    std::vector<Node_transform_operation::Parameters> m_entries;

    erhe::physics::Motion_mode                m_motion_mode  {erhe::physics::Motion_mode::e_kinematic_physical};
    bool                                      m_touched      {false};
    Handle                                    m_hover_handle {Handle::e_handle_none};
    Handle                                    m_active_handle{Handle::e_handle_none};

    Anchor_state                              m_anchor_state_initial;
    glm::mat4                                 m_world_from_anchor{1.0f};
    glm::mat4                                 m_anchor_from_world{1.0f};

    std::shared_ptr<erhe::scene::Node>        m_tool_node;
    std::optional<erhe::physics::Motion_mode> m_original_motion_mode;
    bool                                      m_translate_snap_enable{false};
    bool                                      m_rotate_snap_enable   {false};
    int                                       m_translate_snap_index {2};
    float                                     m_translate_snap       {0.1f};
    int                                       m_rotate_snap_index    {2};
    float                                     m_rotate_snap          {15.0f};

    bool                                 m_cast_rays{true};
    Debug_rendering                      m_debug_rendering;
    Drag                                 m_drag;
    Rotation_context                     m_rotation;
    std::optional<Handle_visualizations> m_visualization;

    float m_debug_scale{1.0f};
};

extern Transform_tool* g_transform_tool;

} // namespace editor