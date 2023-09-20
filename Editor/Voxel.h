#pragma once
#include <memory>
#include <vector>
#include <variant>
#include <FVCore.h>

using namespace FV;

struct VolumeMaterial {
    Color color;
};

struct AABBTree {
    AABB aabb;
    VolumeMaterial* material;
    std::vector<AABBTree> subdivisions;
};


class Voxelizer {

public:
    bool conversionInProgress = false;
};

std::shared_ptr<Voxelizer> voxelize(const std::vector<Triangle>&, int depth);
