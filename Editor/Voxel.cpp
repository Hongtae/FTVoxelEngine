#include <numeric>
#include <chrono>
#include <limits>
#include "Voxel.h"
#include <FVCore.h>

std::shared_ptr<AABBOctree> voxelize(uint32_t maxDepth,
                                     uint64_t numTriangles,
                                     uint64_t baseIndex,
                                     TriangleQuery tq,
                                     MaterialQuery mq) {

    auto start = std::chrono::high_resolution_clock::now();

    auto octrees = AABBOctree::makeTree(maxDepth, numTriangles, baseIndex, tq, mq);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start);

    if (octrees) {
        Log::info(enUS_UTF8,
                  "triangle-octree(depth:{}) generated with nodes:{:Ld}, leaf-nodes:{:Ld}, elapsed:{}",
                  maxDepth,
                  octrees->numDescendants,
                  octrees->numLeafNodes,
                  elapsed.count());
        Log::debug("Nodes: {}, Leaf-Nodes: {}",
                   octrees->_numberOfDescendants(),
                   octrees->_numberOfLeafNodes());
    } else {
        Log::info("No output.");
    }
    return octrees;
}
