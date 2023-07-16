#pragma once

#include <memory>
#include <filesystem>
#include <vector>

namespace erhe::geometry {
    class Geometry;
}
namespace erhe::graphics {
    class Instance;
    class Sampler;
    class Texture;
    class Vertex_format;
}
namespace erhe::primitive {
    class Buffer_sink;
    class Material;
}
namespace erhe::scene {
    class Animation;
    class Camera;
    class Light;
    class Mesh;
    class Node;
    class Scene;
    class Skin;
    using Layer_id = uint64_t;
}

namespace erhe::gltf {

class Image_transfer;

class Gltf_data
{
public:
    std::vector<std::shared_ptr<erhe::scene::Animation>>    animations;
    std::vector<std::shared_ptr<erhe::scene::Camera>>       cameras;
    std::vector<std::shared_ptr<erhe::scene::Light>>        lights;
    std::vector<std::shared_ptr<erhe::scene::Mesh>>         meshes;
    std::vector<std::shared_ptr<erhe::scene::Skin>>         skins;
    std::vector<std::shared_ptr<erhe::scene::Node>>         nodes;
    std::vector<std::shared_ptr<erhe::geometry::Geometry>>  geometries;
    std::vector<std::shared_ptr<erhe::primitive::Material>> materials;
    std::vector<std::shared_ptr<erhe::graphics::Texture>>   images;
    std::vector<std::shared_ptr<erhe::graphics::Sampler>>   samplers;
};

[[nodiscard]] auto parse_gltf(
    erhe::graphics::Instance&                 graphics_instance,
    Image_transfer&                           image_transfer,
    const std::shared_ptr<erhe::scene::Node>& root_node,
    erhe::scene::Layer_id                     mesh_layer_id,
    std::filesystem::path                     path
) -> Gltf_data;

}