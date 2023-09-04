#pragma once
#include "../include.h"
#include <cstdint>
#include <string>
#include <format>
#include <vector>

namespace FV {
    struct CRC32Digest {
        uint32_t hash;
        std::string string() {
            return std::format("{:08x}", hash);
        }
    };

    struct SHA1Digest {
        uint32_t hash[5];
        std::string string() {
            return std::format(
                "{:08x}{:08x}{:08x}{:08x}{:08x}",
                hash[0], hash[1], hash[2], hash[3], hash[4]);
        }
    };

    struct SHA224Digest {
        uint32_t hash[7];
        std::string string() {
            return std::format(
                "{:08x}{:08x}{:08x}{:08x}{:08x}{:08x}{:08x}",
                hash[0], hash[1], hash[2], hash[3],
                hash[4], hash[5], hash[6]);
        }
    };

    struct SHA256Digest {
        uint32_t hash[8];
        std::string string() {
            return std::format(
                "{:08x}{:08x}{:08x}{:08x}{:08x}{:08x}{:08x}{:08x}",
                hash[0], hash[1], hash[2], hash[3],
                hash[4], hash[5], hash[6], hash[7]);
        }
    };

    struct SHA384Digest {
        uint64_t hash[6];
        std::string string() {
            return std::format(
                "{:016x}{:016x}{:016x}{:016x}{:016x}{:016x}",
                hash[0], hash[1], hash[2],
                hash[3], hash[4], hash[5]);
        }
    };

    struct SHA512Digest {
        uint64_t hash[8];
        std::string string() {
            return std::format(
                "{:016x}{:016x}{:016x}{:016x}{:016x}{:016x}{:016x}{:016x}",
                hash[0], hash[1], hash[2], hash[3],
                hash[4], hash[5], hash[6], hash[7]);
        }
    };

    struct _Hash32 {
        uint64_t low;
        std::vector<uint8_t> data;
    };

    struct FVCORE_API CRC32 {
        using Digest = CRC32Digest;

        CRC32();
        void update(const void* data, size_t size);
        Digest finalize();

        static Digest hash(const void* data, size_t size);
    private:
        uint32_t state;
    };

    struct FVCORE_API SHA1 {
        using Digest = SHA1Digest;

        SHA1();
        void update(const void* data, size_t size);
        Digest finalize();

        static Digest hash(const void* data, size_t size);
    private:
        void _update(const uint32_t*);
        uint32_t state[5];
        uint32_t W[80];
        _Hash32 _hash;
    };

    struct FVCORE_API SHA256 {
        using Digest = SHA256Digest;
        SHA256();

        void update(const void* data, size_t size);
        Digest finalize();

        static Digest hash(const void* data, size_t size);
    private:
        void _update(const uint32_t*);
        uint32_t state[8];
        uint32_t W[64];
        _Hash32 _hash;
        friend struct SHA224;
    };

    struct FVCORE_API SHA512 {
        using Digest = SHA512Digest;
        SHA512();

        void update(const void* data, size_t size);
        Digest finalize();

        static Digest hash(const void* data, size_t size);
    private:
        void _update(const uint64_t*);
        uint64_t low;
        uint64_t high;
        uint64_t W[80];
        uint64_t state[8];
        std::vector<uint8_t> data;
        friend struct SHA384;
    };

    struct FVCORE_API SHA224 {
        using Digest = SHA224Digest;
        SHA224();

        void update(const void* data, size_t size);
        Digest finalize();

        static Digest hash(const void* data, size_t size);
    private:
        SHA256 _hash;
    };

    struct FVCORE_API SHA384 {
        using Digest = SHA384Digest;
        SHA384();

        void update(const void* data, size_t size);
        Digest finalize();

        static Digest hash(const void* data, size_t size);
    private:
        SHA512 _hash;
    };
}
