#include "erhe/geometry/operation/triangulate.hpp"
#include "erhe/geometry/geometry.hpp"

#define ERHE_TRACY_NO_GL 1
#include "erhe/toolkit/tracy_client.hpp"

#include <fmt/format.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace erhe::geometry::operation
{

Triangulate::Triangulate(Geometry& src, Geometry& destination)
    : Geometry_operation{src, destination}
{
    ZoneScoped;

    make_points_from_points();
    make_polygon_centroids();

    source.for_each_polygon_const([&](auto& i)
    {
        if (i.polygon.corner_count == 3)
        {
            const Polygon_id new_polygon_id = make_new_polygon_from_polygon(i.polygon_id);
            add_polygon_corners(new_polygon_id, i.polygon_id);
            return;
        }

        i.polygon.for_each_corner_neighborhood_const(source, [&](auto& j)
        {
            const Polygon_id new_polygon_id = destination.make_polygon();
            make_new_corner_from_polygon_centroid(new_polygon_id, i.polygon_id);
            make_new_corner_from_corner          (new_polygon_id, j.corner_id);
            make_new_corner_from_corner          (new_polygon_id, j.next_corner_id);
        });
    });

    post_processing();
}

auto triangulate(Geometry& source) -> Geometry
{
    return Geometry(
        fmt::format("triangulate({})", source.name),
        [&source](auto& result)
        {
            Triangulate operation(source, result);
        }
    );
}


} // namespace erhe::geometry::operation
