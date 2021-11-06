#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <memory>

namespace erhe::scene
{

class Node;
class Mesh;
class Camera;
class Light;

class Layer
{
public:
    std::vector<std::shared_ptr<Mesh>>  meshes;
    std::vector<std::shared_ptr<Light>> lights;
    glm::vec4                           ambient_light{0.0f, 0.0f, 0.0f, 0.0f};
};

class Scene
{
public:
    void sort_transform_nodes  ();
    void update_node_transforms();

    std::vector<std::shared_ptr<Node>>   nodes;
    std::vector<std::shared_ptr<Layer>>  layers;
    std::vector<std::shared_ptr<Camera>> cameras;

    bool nodes_sorted{false};

    auto transform_update_serial() -> uint64_t;

private:
    uint64_t m_transform_update_serial{0};
};

} // namespace erhe::scene
