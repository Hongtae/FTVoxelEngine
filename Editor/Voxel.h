#pragma once
#include <memory>
#include <vector>
#include <variant>
#include <functional>
#include <FVCore.h>

using namespace FV;

class Voxelizer {

public:
    bool conversionInProgress = false;
};

using TriangleQuery = std::function<const Triangle& (uint64_t)>;
using PayloadQuery = std::function<uint64_t(uint64_t)>;

std::shared_ptr<AABBOctree> voxelize(uint64_t numTriangles, int depth, TriangleQuery, PayloadQuery);
