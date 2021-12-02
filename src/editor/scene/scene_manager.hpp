#pragma once

#include "scene/collision_generator.hpp"
#include "scene/frame_controller.hpp"
#include "windows/brushes.hpp"

#include "erhe/components/component.hpp"

#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

class btCollisionShape;

namespace erhe::geometry
{
    class Geometry;
}

namespace erhe::graphics
{
    class Buffer;
    class Buffer_transfer_queue;
    class Vertex_format;
}

namespace erhe::primitive
{
    class Primitive;
    class Primitive_eometry;
}

namespace erhe::scene
{
    class Camera;
    class ICamera;
    class Light;
    class Mesh;
    class Mesh_layer;
    class Scene;
}

namespace editor
{

class Brush;
class Debug_draw;
class Mesh_memory;
class Node_physics;
class Scene_root;

class Scene_manager
    : public erhe::components::Component
    , public erhe::components::IUpdate_fixed_step
    , public erhe::components::IUpdate_once_per_frame
{
public:
    static constexpr std::string_view c_name{"Scene_manager"};
    static constexpr uint32_t hash = compiletime_xxhash::xxh32(c_name.data(), c_name.size(), {});

    Scene_manager();
    ~Scene_manager() override;

    // Implements Component
    auto get_type_hash       () const -> uint32_t override { return hash; }
    void connect             () override;
    void initialize_component() override;

    void set_view_camera(std::shared_ptr<erhe::scene::ICamera> camera);
    auto get_view_camera() const -> std::shared_ptr<erhe::scene::ICamera>;
    void setup_scene    ();
    void sort_lights    ();
    auto make_camera(
        std::string_view name,
        glm::vec3        position,
        glm::vec3        look_at = glm::vec3{0.0f, 0.0f, 0.0f}
    ) -> std::shared_ptr<erhe::scene::Camera>;
    auto make_directional_light(
        std::string_view name,
        glm::vec3        position,
        glm::vec3        color,
        float            intensity
    ) -> std::shared_ptr<erhe::scene::Light>;

    auto make_spot_light(
        std::string_view name,
        glm::vec3        position,
        glm::vec3        direction,
        glm::vec3        color,
        float            intensity,
        glm::vec2        spot_cone_angle
    ) -> std::shared_ptr<erhe::scene::Light>;

    template <typename ...Args>
    auto make_brush(
        bool instantiate_to_scene,
        Args&& ...args
    ) -> std::shared_ptr<Brush>
    {
        auto brush = m_brushes->make_brush(std::forward<Args>(args)...);
        if (instantiate_to_scene)
        {
            std::lock_guard<std::mutex> lock(m_scene_brushes_mutex);
            m_scene_brushes.push_back(brush);
        }
        return brush;
    }

    // Implements IUpdate_once_per_frame
    void update_once_per_frame(const erhe::components::Time_context& time_context) override;

    // Implements IUpdate_fixed_step
    void update_fixed_step(const erhe::components::Time_context& time_context) override;

private:
    auto build_info_set       () -> erhe::primitive::Build_info_set&;
    void setup_cameras        ();
    void animate_lights       (double time_d);
    auto buffer_transfer_queue() -> erhe::graphics::Buffer_transfer_queue&;
    void add_floor            ();
    void make_brushes         ();
    void make_mesh_nodes      ();
    void make_cube_benchmark  ();
    void setup_lights         ();

    // Components
    Brushes*     m_brushes    {nullptr};
    Mesh_memory* m_mesh_memory{nullptr};
    Scene_root*  m_scene_root {nullptr};

    // Self owned parts
    std::shared_ptr<erhe::scene::ICamera> m_view_camera;
    //std::shared_ptr<erhe::scene::ICamera> m_view_camera_position_only;
    //std::shared_ptr<erhe::scene::ICamera> m_view_camera_position_heading;
    //std::shared_ptr<erhe::scene::ICamera> m_view_camera_position_heading_elevation;
    std::mutex                            m_brush_mutex;
    std::unique_ptr<Brush>                m_floor_brush;
    std::mutex                            m_scene_brushes_mutex;
    std::vector<std::shared_ptr<Brush>>   m_scene_brushes;
    //std::shared_ptr<erhe::scene::Camera>  m_camera;
    //std::shared_ptr<Frame_controller>     m_camera_controls;

    std::vector<std::shared_ptr<erhe::physics::ICollision_shape>> m_collision_shapes;
};

} // namespace editor
