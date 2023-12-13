#pragma once
#include <memory>
#include <vector>
#include <unordered_set>
#include <optional>
#include <mutex>

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV {

    struct VulkanMemoryBlock final {
        uint64_t offset;
        uint64_t size;
        class VulkanMemoryChunk* chunk;
    };

    class VulkanGraphicsDevice;
    class VulkanMemoryChunk final {
    public:
        VulkanMemoryChunk(VulkanGraphicsDevice*,
                          class VulkanMemoryPool*,
                          class VulkanMemoryAllocator*,
                          VkDeviceMemory,
                          VkMemoryPropertyFlags,
                          uint64_t, uint64_t, uint64_t,
                          bool dedicated);
        ~VulkanMemoryChunk();

        const uint64_t chunkSize;
        const uint64_t blockSize;
        const uint64_t totalBlocks;
        const bool dedicated;
        void* mapped;

        const VkMemoryPropertyFlags propertyFlags;
        const VkDeviceMemory memory;

        class VulkanMemoryPool* const pool;
        class VulkanMemoryAllocator* const allocator;

        bool invalidate(uint64_t offset, uint64_t size) const;
        bool flush(uint64_t offset, uint64_t size) const;

        void push(const VulkanMemoryBlock&);
        std::optional<VulkanMemoryBlock> pop();

        size_t numFreeBlocks() const { return freeBlocks.size(); }

    private:
        std::vector<VulkanMemoryBlock> freeBlocks;
        VulkanGraphicsDevice* const gdevice;
    };

    class VulkanMemoryAllocator final {
    public:
        VulkanMemoryAllocator(class VulkanMemoryPool*, uint64_t, uint64_t);
        ~VulkanMemoryAllocator();

        uint64_t numAllocations() const;
        uint64_t numDeviceAllocations() const;

        uint64_t totalMemorySize() const;
        uint64_t memorySizeInUse() const;

        std::optional<VulkanMemoryBlock> alloc(uint64_t);
        void dealloc(VulkanMemoryBlock&);

        uint64_t purge();

        class VulkanMemoryPool* const pool;

        const uint64_t blockSize;
        const uint64_t blocksPerChunk;

    private:
        uint64_t memoryInUse;
        std::vector<VulkanMemoryChunk*> chunks;
        mutable std::mutex mutex;
    };

    class VulkanMemoryPool final {
    public:
        VulkanMemoryPool(VulkanGraphicsDevice* device,
                         uint32_t typeIndex,
                         const VkMemoryPropertyFlags& flags,
                         const VkMemoryHeap& heap);
        ~VulkanMemoryPool();

        const uint32_t memoryTypeIndex;
        const VkMemoryPropertyFlags memoryPropertyFlags;
        const VkMemoryHeap memoryHeap;

        std::optional<VulkanMemoryBlock> alloc(uint64_t size);
        std::optional<VulkanMemoryBlock> allocDedicated(uint64_t size, VkImage, VkBuffer);
        void dealloc(VulkanMemoryBlock&);

        uint64_t purge();

        uint64_t numAllocations() const;
        uint64_t numDeviceAllocations() const;
        uint64_t totalMemorySize() const;
        uint64_t memorySizeInUse() const;

        class VulkanGraphicsDevice* const gdevice;

    private:
        std::vector<VulkanMemoryAllocator*> allocators;

        mutable std::mutex mutex;
        std::unordered_set<VulkanMemoryChunk*> dedicatedAllocations;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
