#pragma once
#include <memory>
#include <vector>
#include <variant>
#include <functional>
#include <FVCore.h>

using namespace FV;


using TriangleQuery = AABBOctree::TriangleQuery;
using MaterialQuery = AABBOctree::MaterialQuery;

std::shared_ptr<AABBOctree> voxelize(uint32_t maxDepth,
                                     uint64_t numTriangles,
                                     uint64_t baseIndex,
                                     TriangleQuery tq,
                                     MaterialQuery mq);
