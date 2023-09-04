#include <algorithm>
#include "../Libs/dkwrapper/DKCompression.h"
#include "Compression.h"
#include "Logger.h"

namespace FV {

    const CompressionMethod CompressionMethod::fastest = { CompressionAlgorithm::Lz4, 0 };
    const CompressionMethod CompressionMethod::fast = { CompressionAlgorithm::Lz4, 9 };
    const CompressionMethod CompressionMethod::best = { CompressionAlgorithm::Lzma, 9 };
    const CompressionMethod CompressionMethod::balance = { CompressionAlgorithm::Zstd, 3 };
    const CompressionMethod CompressionMethod::automatic = { CompressionAlgorithm::Automatic, 0 };

    CompressionResult FVCORE_API compress(std::istream& input, std::ostream& output, CompressionMethod method, size_t inputBytes) {
        struct InputContext {
            std::istream& stream;
            std::streampos offset;
            std::streampos end;
            InputContext(std::istream& s, uint64_t size)
                : stream(s), offset(s.tellg()) {
                stream.seekg(0, std::ios::end);
                auto streamEnd = stream.tellg();
                stream.clear();
                stream.seekg(offset);

                auto length = streamEnd - offset;
                auto inputBytes = std::min((std::streamoff)size, length);

                end = offset + inputBytes;
                FVASSERT_DEBUG(end <= streamEnd);
            }
        };
        InputContext inContext(input, inputBytes);
        DKStream inStream = {};
        inStream.userContext = &inContext;
        inStream.read = [](void* p, void* buffer, size_t size)->uint64_t {
            InputContext* ctx = (InputContext*)p;
            auto remains = ctx->end - ctx->stream.tellg();
            size = std::min((std::streamoff)size, remains);
            if (size > 0) {
                ctx->stream.read((char*)buffer, size);
                return ctx->stream.gcount();
            }
            return 0;
        };
        inStream.remainLength = [](void* p)->uint64_t {
            InputContext* ctx = (InputContext*)p;
            return ctx->end - ctx->stream.tellg();
        };

        struct OutputContext {
            std::ostream& stream;
        };
        OutputContext outContext{ output };
        DKStream outStream = {};
        outStream.userContext = &outContext;
        outStream.write = [](void* p, const void* data, size_t size)->uint64_t {
            OutputContext* ctx = (OutputContext*)p;
            if (ctx->stream.good() == false)
                return ~uint64_t(0); // error

            auto current = ctx->stream.tellp();
            try {
                ctx->stream.write((const char*)data, size);
            } catch (const std::exception& exp) {
                Log::error(std::format("stream write failed: {}", exp.what()));
            }
            auto written = ctx->stream.tellp() - current;
            return written;
        };

        int level = method.level;
        DKCompressionAlgorithm algo = DKCompressionAlgorithm_Zstd;
        switch (method.algorithm) {
        case CompressionAlgorithm::Zlib:    algo = DKCompressionAlgorithm_Zlib; break;
        case CompressionAlgorithm::Zstd:    algo = DKCompressionAlgorithm_Zstd; break;
        case CompressionAlgorithm::Lz4:     algo = DKCompressionAlgorithm_Lz4; break;
        case CompressionAlgorithm::Lzma:    algo = DKCompressionAlgorithm_Lzma; break;
        case CompressionAlgorithm::Automatic:
            algo = DKCompressionAlgorithm_Zstd;
            level = 3;
        }

        auto result = DKCompressionEncode(algo, &inStream, &outStream, level);
        return (CompressionResult)result;
    }

    CompressionResult FVCORE_API decompress(std::istream& input, std::ostream& output, CompressionAlgorithm algorithm) {
        struct InputContext {
            std::istream& stream;
        };
        InputContext inContext = { input };
        DKStream inStream = {};
        inStream.userContext = &inContext;
        inStream.read = [](void* p, void* buffer, size_t size)->uint64_t {
            InputContext* ctx = (InputContext*)p;
            if (ctx->stream.good()) {
                ctx->stream.read((char*)buffer, size);
                return ctx->stream.gcount();
            }
            return ~uint64_t(0);
        };

        struct OutputContext {
            std::ostream& stream;
        };
        OutputContext outContext = { output };
        DKStream outStream = {};
        outStream.userContext = &outContext;
        outStream.write = [](void* p, const void* data, size_t size)->uint64_t {
            OutputContext* ctx = (OutputContext*)p;
            if (ctx->stream.good() == false)
                return ~uint64_t(0); // error

            auto current = ctx->stream.tellp();
            try {
                ctx->stream.write((const char*)data, size);
            } catch (const std::exception& exp) {
                Log::error(std::format("stream write failed: {}", exp.what()));
            }
            auto written = ctx->stream.tellp() - current;
            return written;
        };

        DKCompressionAlgorithm algo;
        DKCompressionResult result;
        if (algorithm == CompressionAlgorithm::Automatic) {
            result = DKCompressionDecodeAutoDetect(&inStream, &outStream, &algo);
            return (CompressionResult)result;
        }

        switch (algorithm) {
        case CompressionAlgorithm::Zlib:    algo = DKCompressionAlgorithm_Zlib; break;
        case CompressionAlgorithm::Zstd:    algo = DKCompressionAlgorithm_Zstd; break;
        case CompressionAlgorithm::Lz4:     algo = DKCompressionAlgorithm_Lz4; break;
        case CompressionAlgorithm::Lzma:    algo = DKCompressionAlgorithm_Lzma; break;
        default:
            break;
        }
        result = DKCompressionDecode(algo, &inStream, &outStream);
        return (CompressionResult)result;
    }
}
