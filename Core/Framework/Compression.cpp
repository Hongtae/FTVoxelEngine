#include "../Libs/dkwrapper/DKCompression.h"
#include "Compression.h"


namespace FV
{

    const CompressionMethod CompressionMethod::fastest = { CompressionAlgorithm::Lz4, 0 };
    const CompressionMethod CompressionMethod::fast = { CompressionAlgorithm::Lz4, 9 };
    const CompressionMethod CompressionMethod::best = { CompressionAlgorithm::Lzma, 9 };
    const CompressionMethod CompressionMethod::balance = { CompressionAlgorithm::Zstd, 3 };
    const CompressionMethod CompressionMethod::automatic = { CompressionAlgorithm::Automatic, 0 };

    CompressionResult FVCORE_API compress(std::istream input, size_t, std::ostream, CompressionMethod method)
    {
        return CompressionResult::UnknownError;
    }

    CompressionResult FVCORE_API decompress(std::istream input, std::ostream, CompressionAlgorithm algorithm)
    {
        return CompressionResult::UnknownError;
    }
}
