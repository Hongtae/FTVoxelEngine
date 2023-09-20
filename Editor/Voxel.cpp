#include "Voxel.h"
#include <FVCore.h>

struct TriangleOctree {
    AABB aabb;
    std::vector<Triangle> triangles;
    std::vector<TriangleOctree> subdivisions;

    void subdivide(int depthLevel) {
        if (depthLevel <= 0)
            return;

        auto extent = aabb.extent();
        auto halfExt = extent * 0.5f;
        AABB div[8];
        for (int x : {0, 1}) {
            for (int y : {0, 1}) {
                for (int z : {0, 1}) {
                    AABB& aabb = div[x * 4 + y * 2 + z];
                    aabb.min = this->aabb.min;
                    aabb.max = aabb.min + halfExt;
                    aabb.min.x += halfExt.x * x;
                    aabb.max.x += halfExt.x * x;
                    aabb.min.y += halfExt.y * y;
                    aabb.max.y += halfExt.y * y;
                    aabb.min.z += halfExt.z * z;
                    aabb.max.z += halfExt.z * z;
                }
            }
        }
        for (const AABB& aabb : div) {
            Log::debug(std::format("AABB min:{} max:{}", aabb.min, aabb.max));
        }
    }
};


std::shared_ptr<Voxelizer> voxelize(const std::vector<Triangle>& triangles, int depth) {
    AABB aabb{};
    for (auto& tri : triangles) {
        aabb.expand({ tri.pos[0], tri.pos[1], tri.pos[2] });
    }
    if (aabb.isValid() == false)
        return nullptr;

    Vector3 scale = aabb.extent();
    for (float& s : scale.val) {
        if (s == 0.0f) s = 1.0;
    }

    Matrix4 quantize = AffineTransform3::identity.scaled(scale).matrix4();
    Matrix4 normalize = quantize.inverted();

    TriangleOctree normalizedOctree{
        AABB(Vector3(0,0,0),Vector3(1,1,1)),
        triangles,
        {}
    };
    normalizedOctree.subdivide(std::max(depth, 0));

    return std::make_shared<Voxelizer>();
}
