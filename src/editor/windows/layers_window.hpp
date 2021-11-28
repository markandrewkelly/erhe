#pragma once

#include "windows/imgui_window.hpp"

#include <glm/glm.hpp>

#include <memory>

namespace erhe::graphics
{
    class Texture;
}

namespace erhe::scene
{
    class Node;
    enum class Light_type : unsigned int;
}

namespace editor
{

class Icon_set;
class Scene_root;
class Selection_tool;

class Layers_window
    : public erhe::components::Component
    , public Imgui_window
{
public:
    static constexpr std::string_view c_name {"Layers_window"};
    static constexpr std::string_view c_title{"Layers"};
    static constexpr uint32_t hash = compiletime_xxhash::xxh32(c_name.data(), c_name.size(), {});

    Layers_window ();
    ~Layers_window() override;

    // Implements Component
    auto get_type_hash       () const -> uint32_t override { return hash; }
    void connect             () override;
    void initialize_component() override;

    // Implements Imgui_window
    void imgui(Pointer_context& pointer_context) override;

private:
    auto get_icon(const erhe::scene::Light_type type) const -> const ImVec2;

    Scene_root*     m_scene_root    {nullptr};
    Selection_tool* m_selection_tool{nullptr};
    Icon_set*       m_icon_set      {nullptr};

    std::shared_ptr<erhe::scene::Node> m_node_clicked;
};

} // namespace editor