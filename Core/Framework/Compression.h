#pragma once
#include "../include.h"
#include <istream>
#include <ostream>

namespace FV
{
    enum class CompressionAlgorithm
    {
        Zlib,
        Zstd,
        Lz4,
        Lzma,
        Automatic, // Default method for compression, Auto-detected method for decompression.
    };

    struct FVCORE_API CompressionMethod
    {
        CompressionAlgorithm algorithm;
        int level;

        static const CompressionMethod fastest;
        static const CompressionMethod fast;
        static const CompressionMethod best;
        static const CompressionMethod balance;
        static const CompressionMethod automatic;
    };

    enum class CompressionResult
    {
        Success = 0,
        UnknownError,
        OutOfMemory,
        InputStreamError,
        OutputStreamError,
        DataError,
        InvalidParameter,
        UnknownFormat,
    };

    CompressionResult FVCORE_API compress(std::istream& input, std::ostream&, CompressionMethod method = CompressionMethod::automatic, size_t inputBytes = size_t(-1));
    CompressionResult FVCORE_API decompress(std::istream& input, std::ostream&, CompressionAlgorithm algorithm = CompressionAlgorithm::Automatic);
}

