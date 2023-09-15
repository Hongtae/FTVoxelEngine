#pragma once
#include <memory>
#include <vector>
#include <FVCore.h>

using namespace FV;

class Voxelizer {

public:
    bool conversionInProgress = false;
};

struct Triangle {
    Vector3 pos[3];
};

std::shared_ptr<Voxelizer> voxelize(const std::vector<Triangle>&, int depth);
