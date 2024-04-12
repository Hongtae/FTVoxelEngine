#include "../../Logger.h"
#include "VulkanExtensions.h"
#include "VulkanGraphicsDevice.h"
#include "VulkanDeviceMemory.h"

#if FVCORE_ENABLE_VULKAN

namespace {
    constexpr struct {
        uint64_t blockSize;
        uint64_t numBlocks;
    } memoryChunkSizeBlocks[] = {
    { 1024, 512 },
    { 2048, 512 },
    { 4096, 512 },
    { 8192, 512 },
    { 16384, 256 },
    { 32768, 256 },
    { 65536, 256 },
    { 131072, 256 },
    { 262144, 256 },
    { 524288, 256 },
    { 1048576, 128 },
    { 2097152, 64 },
    { 4194304, 32 },
    { 8388608, 16 },
    { 16777216, 8 },
    { 33554432, 4 },
    };
}

using namespace FV;

VulkanMemoryChunk::VulkanMemoryChunk(VulkanGraphicsDevice* dev,
                                     VulkanMemoryPool* pool,
                                     VulkanMemoryAllocator* allocator,
                                     VkDeviceMemory mem,
                                     VkMemoryPropertyFlags flags,
                                     uint64_t chunkSize,
                                     uint64_t blockSize,
                                     uint64_t totalBlocks,
                                     bool dedicated)
    : gdevice(dev)
    , propertyFlags(flags)
    , memory(mem)
    , chunkSize(chunkSize)
    , blockSize(blockSize)
    , totalBlocks(totalBlocks)
    , dedicated(dedicated)
    , mapped(nullptr)
    , pool(pool)
    , allocator(allocator) {

    FVASSERT_DEBUG(memory != VK_NULL_HANDLE);

    if (propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        VkDeviceSize offset = 0;
        VkDeviceSize size = VK_WHOLE_SIZE;

        auto device = gdevice->device;
        VkResult err = vkMapMemory(device, memory, offset, size, 0, &mapped);
        if (err != VK_SUCCESS) {
            Log::error("vkMapMemory failed: {}", err);
        }
    }

    freeBlocks.reserve(totalBlocks);
    uint64_t offset = 0;
    for (uint64_t i = 0; i < totalBlocks; ++i) {
        auto block = VulkanMemoryBlock{ offset, blockSize, this };
        freeBlocks.push_back(block);
        offset += blockSize;
    }
}

VulkanMemoryChunk::~VulkanMemoryChunk() {
    auto device = gdevice->device;
    auto allocationCallbacks = gdevice->allocationCallbacks();

    FVASSERT_DEBUG(freeBlocks.size() == totalBlocks);
    FVASSERT_DEBUG(memory != VK_NULL_HANDLE);
    if (mapped)
        vkUnmapMemory(device, memory);
    vkFreeMemory(device, memory, allocationCallbacks);
}

void VulkanMemoryChunk::push(const VulkanMemoryBlock& block) {
    FVASSERT_DEBUG(block.chunk == this);
    FVASSERT_DEBUG(block.size <= blockSize);
    FVASSERT_DEBUG(block.offset < chunkSize);

    auto copy = block;
    copy.size = blockSize;
    freeBlocks.push_back(copy);
    FVASSERT_DEBUG(freeBlocks.size() <= totalBlocks);
}

std::optional<VulkanMemoryBlock> VulkanMemoryChunk::pop() {
    if (freeBlocks.empty() == false) {
        auto block = freeBlocks.back();
        freeBlocks.pop_back();
        return block;
    }
    return {};
}

bool VulkanMemoryChunk::invalidate(uint64_t offset, uint64_t size) const {
    auto device = gdevice->device;
    FVASSERT_DEBUG(memory != VK_NULL_HANDLE);
    if (mapped && propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        if (offset < chunkSize) {
            VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
            range.memory = memory;
            range.offset = offset;
            if (size == VK_WHOLE_SIZE)
                range.size = size;
            else
                range.size = std::min(size, chunkSize - offset);
            VkResult err = vkInvalidateMappedMemoryRanges(device, 1, &range);
            if (err == VK_SUCCESS) {
                return true;
            } else {
                Log::error("vkInvalidateMappedMemoryRanges failed: {}", err);
            }
        } else {
            Log::error("VulkanMemoryChunk::invalidate() failed: Out of range");
        }
    }
    return false;
}

bool VulkanMemoryChunk::flush(uint64_t offset, uint64_t size) const {
    auto device = gdevice->device;
    FVASSERT_DEBUG(memory != VK_NULL_HANDLE);
    if (mapped && propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        if (offset < chunkSize) {
            VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
            range.memory = memory;
            range.offset = offset;
            if (size == VK_WHOLE_SIZE)
                range.size = size;
            else
                range.size = std::min(size, chunkSize - offset);
            VkResult err = vkFlushMappedMemoryRanges(device, 1, &range);
            if (err == VK_SUCCESS) {
                return true;
            } else {
                Log::error("vkFlushMappedMemoryRanges failed: {}", err);
            }
        } else {
            Log::error("VulkanMemoryChunk::flush() failed: Out of range");
        }
    }
    return false;
}

VulkanMemoryAllocator::VulkanMemoryAllocator(VulkanMemoryPool* pool,
                                             uint64_t s, uint64_t n)
    : blockSize(s)
    , blocksPerChunk(n)
    , pool(pool) {
}

VulkanMemoryAllocator::~VulkanMemoryAllocator() {
    for (auto chunk : chunks) {
        delete chunk;
    }
}

uint64_t VulkanMemoryAllocator::numAllocations() const {
    uint64_t blocks = 0;
    std::scoped_lock lock(mutex);
    for (auto chunk : chunks) {
        uint64_t n = chunk->totalBlocks - chunk->numFreeBlocks();
        FVASSERT_DEBUG(n <= chunk->totalBlocks);
        blocks += n;
    }
    return blocks;
}

uint64_t VulkanMemoryAllocator::numDeviceAllocations() const {
    std::scoped_lock lock(mutex);
    return chunks.size();
}

uint64_t VulkanMemoryAllocator::totalMemorySize() const {
    uint64_t size = 0;
    std::scoped_lock lock(mutex);
    for (auto chunk : chunks) {
        size += chunk->chunkSize;
    }
    return size;
}

uint64_t VulkanMemoryAllocator::memorySizeInUse() const {
    std::scoped_lock lock(mutex);
    return memoryInUse;
}

std::optional<VulkanMemoryBlock> VulkanMemoryAllocator::alloc(uint64_t size) {
    if (size > blockSize)
        return {};

    std::scoped_lock lock(mutex);
    for (auto chunk : chunks) {
        if (chunk->numFreeBlocks() > 0) {
            auto item = chunk->pop();
            FVASSERT_DEBUG(item);
            FVASSERT_DEBUG(item.value().size >= size);
            item.value().size = size;
            memoryInUse += size;
            return item;
        }
    }
    // allocate new chunk
    auto gdevice = pool->gdevice;
    VkDevice device = gdevice->device;
    uint32_t memoryTypeIndex = pool->memoryTypeIndex;
    auto memoryPropertyFlags = pool->memoryPropertyFlags;
    auto allocationCallbacks = gdevice->allocationCallbacks();

    uint64_t chunkSize = blockSize * blocksPerChunk;

    VkMemoryAllocateInfo memAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memAllocInfo.allocationSize = chunkSize;
    memAllocInfo.memoryTypeIndex = memoryTypeIndex;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkResult err = vkAllocateMemory(device, &memAllocInfo, allocationCallbacks, &memory);
    if (err != VK_SUCCESS) {
        Log::error("vkAllocateMemory failed: {}", err);
        return {};
    }

    auto chunk = new VulkanMemoryChunk(gdevice, pool, this,
                                       memory, memoryPropertyFlags,
                                       chunkSize, blockSize, blocksPerChunk,
                                       false);
    chunks.push_back(chunk);
    auto block = chunk->pop();
    FVASSERT_DEBUG(block);
    FVASSERT_DEBUG(block.value().size >= size);
    block.value().size = size;
    memoryInUse += size;
    return block;
}

void VulkanMemoryAllocator::dealloc(VulkanMemoryBlock& block) {
    if (block.chunk) {
        auto chunk = block.chunk;
        std::scoped_lock lock(mutex);
        FVASSERT_DEBUG(chunk);
        FVASSERT_DEBUG(chunk->allocator == this);
        FVASSERT_DEBUG(chunk->blockSize == this->blockSize);
        FVASSERT_DEBUG(memoryInUse >= block.size);
        memoryInUse -= block.size;
        chunk->push(block);
        block = {};

        if (chunk->numFreeBlocks() == chunk->totalBlocks) {
            size_t freeBlocks = 0;
            for (VulkanMemoryChunk* c : chunks)
                freeBlocks += c->numFreeBlocks();
            if (freeBlocks > blocksPerChunk + (blocksPerChunk >> 2)) {
                for (auto it = chunks.begin(); it != chunks.end(); ++it) {
                    if (*it == chunk) {
                        chunks.erase(it);
                        break;
                    }
                }
                delete chunk;
            }
        }
        std::sort(
            chunks.begin(), chunks.end(), [](auto a, auto b) {
                return a->numFreeBlocks() < b->numFreeBlocks();
            });
    }
}

uint64_t VulkanMemoryAllocator::purge() {
    uint64_t purged = 0;
    std::scoped_lock lock(mutex);
    while (true) {
        auto iter = std::find_if(
            chunks.begin(), chunks.end(),
            [](auto chunk) {
                return chunk->numFreeBlocks() == chunk->totalBlocks;
            });
        if (iter == chunks.end())
            break;

        auto chunk = (*iter);
        purged += chunk->chunkSize;
        chunks.erase(iter);
        delete chunk;
    }
    return purged;
}

VulkanMemoryPool::VulkanMemoryPool(VulkanGraphicsDevice* device,
                                   uint32_t typeIndex,
                                   const VkMemoryPropertyFlags& flags,
                                   const VkMemoryHeap& heap)
    : memoryTypeIndex(typeIndex)
    , memoryPropertyFlags(flags)
    , memoryHeap(heap)
    , gdevice(device) {

    allocators.reserve(std::size(memoryChunkSizeBlocks));
    for (const auto& chunk : memoryChunkSizeBlocks) {
        auto allocator = new VulkanMemoryAllocator(this,
                                                   chunk.blockSize,
                                                   chunk.numBlocks);

        allocators.push_back(allocator);
    }

    FVASSERT_DEBUG(std::is_sorted(allocators.begin(), allocators.end(),
                                  [](auto& a, auto& b) {
                                      return a->blockSize < b->blockSize;
                                  }));
}

VulkanMemoryPool::~VulkanMemoryPool() {
    for (auto alloc : allocators) {
        delete alloc;
    }
    FVASSERT_DEBUG(dedicatedAllocations.empty());
}

std::optional<VulkanMemoryBlock> VulkanMemoryPool::alloc(uint64_t size) {
    FVASSERT_DEBUG(size > 0);
    if (auto iter = std::lower_bound(allocators.begin(), allocators.end(),
                                     size,
                                     [](VulkanMemoryAllocator* a, uint64_t value) {
                                         return a->blockSize < value;
                                     }); iter != allocators.end()) {
        FVASSERT_DEBUG((*iter)->blockSize >= size);
        return (*iter)->alloc(size);
    }

    auto device = gdevice->device;
    auto allocationCallbacks = gdevice->allocationCallbacks();

    VkMemoryAllocateInfo memAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memAllocInfo.allocationSize = size;
    memAllocInfo.memoryTypeIndex = memoryTypeIndex;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkResult err = vkAllocateMemory(device, &memAllocInfo, allocationCallbacks, &memory);
    if (err != VK_SUCCESS) {
        Log::error("vkAllocateMemory failed: {}", err);
        return {};
    }
    auto chunk = new VulkanMemoryChunk(gdevice, this, nullptr,
                                       memory, memoryPropertyFlags,
                                       size, size, 1,
                                       false);
    std::unique_lock lock(mutex);
    auto it = dedicatedAllocations.insert(chunk);
    FVASSERT_DEBUG(it.second);
    return chunk->pop();
}

std::optional<VulkanMemoryBlock> VulkanMemoryPool::allocDedicated(uint64_t size, VkImage image, VkBuffer buffer) {
    if (image != VK_NULL_HANDLE && buffer != VK_NULL_HANDLE) {
        Log::error("At least one of image and buffer must be VK_NULL_HANDLE");
        return {};
    }
    FVASSERT_DEBUG(size > 0);

    auto device = gdevice->device;
    auto allocationCallbacks = gdevice->allocationCallbacks();

    VkMemoryAllocateInfo memAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memAllocInfo.allocationSize = size;
    memAllocInfo.memoryTypeIndex = memoryTypeIndex;

    VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo = {
            VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO
    };
    memoryDedicatedAllocateInfo.image = image;
    memoryDedicatedAllocateInfo.buffer = buffer;
    memAllocInfo.pNext = &memoryDedicatedAllocateInfo;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkResult err = vkAllocateMemory(device, &memAllocInfo, allocationCallbacks, &memory);
    if (err != VK_SUCCESS) {
        Log::error("vkAllocateMemory failed: {}", err);
        return {};
    }
    auto chunk = new VulkanMemoryChunk(gdevice, this, nullptr,
                                       memory, memoryPropertyFlags,
                                       size, size, 1,
                                       true);
    std::unique_lock lock(mutex);
    auto it = dedicatedAllocations.insert(chunk);
    FVASSERT_DEBUG(it.second);
    return chunk->pop();
}

void VulkanMemoryPool::dealloc(VulkanMemoryBlock& block) {
    auto chunk = block.chunk;
    if (chunk) {
        FVASSERT_DEBUG(chunk->pool == this);
        if (chunk->allocator) {
            auto allocator = chunk->allocator;
            FVASSERT_DEBUG(allocator->pool == this);
            allocator->dealloc(block);
        } else {
            auto device = gdevice->device;
            auto allocationCallbacks = gdevice->allocationCallbacks();

            std::unique_lock lock(mutex);
            FVASSERT_DEBUG(dedicatedAllocations.contains(chunk));
            dedicatedAllocations.erase(chunk);
            lock.unlock();

            chunk->push(block);
            delete chunk;

            block = {};
        }
    }
}

uint64_t VulkanMemoryPool::purge() {
    return std::reduce(
        allocators.begin(), allocators.end(), uint64_t(0),
        [](uint64_t result, auto alloc) { return result + alloc->purge(); });
}

uint64_t VulkanMemoryPool::numAllocations() const {
    uint64_t count = 0;
    for (auto alloc : allocators)
        count += alloc->numAllocations();
    std::unique_lock lock(mutex);
    return count + dedicatedAllocations.size();
}

uint64_t VulkanMemoryPool::numDeviceAllocations() const {
    uint64_t count = 0;
    for (auto alloc : allocators)
        count += alloc->numDeviceAllocations();
    std::unique_lock lock(mutex);
    return count + dedicatedAllocations.size();
}

uint64_t VulkanMemoryPool::totalMemorySize() const {
    uint64_t count = 0;
    for (auto alloc : allocators)
        count += alloc->totalMemorySize();
    std::unique_lock lock(mutex);
    for (auto& chunk : dedicatedAllocations)
        count += chunk->chunkSize;
    return count;
}

uint64_t VulkanMemoryPool::memorySizeInUse() const {
    uint64_t count = 0;
    for (auto alloc : allocators)
        count += alloc->memorySizeInUse();
    std::unique_lock lock(mutex);
    for (auto& chunk : dedicatedAllocations)
        count += chunk->chunkSize;
    return count;
}

#endif //#if FVCORE_ENABLE_VULKAN
