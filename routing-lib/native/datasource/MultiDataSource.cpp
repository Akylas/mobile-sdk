#include "MultiDataSource.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

// gzip decompression via zlib
#include <zlib.h>

// zstd decompression (optional)
#ifdef HAVE_ZSTD
#  include <zstd.h>
#endif

namespace routing {

    MultiDataSource::MultiDataSource(std::vector<std::shared_ptr<IDataSource>> sources)
        : _sources(std::move(sources)) {}

    void MultiDataSource::addSource(std::shared_ptr<IDataSource> source) {
        std::lock_guard<std::mutex> lk(_mutex);
        _sources.push_back(std::move(source));
    }

    bool MultiDataSource::removeSource(const std::shared_ptr<IDataSource>& source) {
        std::lock_guard<std::mutex> lk(_mutex);
        auto it = std::find(_sources.begin(), _sources.end(), source);
        if (it == _sources.end()) return false;
        _sources.erase(it);
        return true;
    }

    std::vector<std::shared_ptr<IDataSource>> MultiDataSource::getSources() const {
        std::lock_guard<std::mutex> lk(_mutex);
        return _sources;
    }

    std::optional<std::vector<uint8_t>> MultiDataSource::getTile(int zoom, int x, int y) {
        auto sources = getSources(); // snapshot
        for (auto& src : sources) {
            auto raw = src->getTile(zoom, x, y);
            if (raw && !raw->empty()) {
                return decompress(*raw);
            }
        }
        return std::nullopt;
    }

    std::string MultiDataSource::getMetadata(const std::string& key) {
        auto sources = getSources();
        for (auto& src : sources) {
            std::string val = src->getMetadata(key);
            if (!val.empty()) return val;
        }
        return {};
    }

    // ---------------------------------------------------------------------------
    // Decompression helpers
    // ---------------------------------------------------------------------------

    static std::vector<uint8_t> tryInflateGzip(const std::vector<uint8_t>& data) {
        // Quick magic check: gzip magic bytes 0x1f 0x8b
        if (data.size() < 2 || data[0] != 0x1f || data[1] != 0x8b) {
            return {};
        }

        z_stream zs{};
        // inflateInit2 with windowBits=47 handles both zlib and gzip
        if (inflateInit2(&zs, 47) != Z_OK) return {};

        std::vector<uint8_t> out;
        out.resize(data.size() * 4);
        zs.next_in  = const_cast<Bytef*>(data.data());
        zs.avail_in = static_cast<uInt>(data.size());

        int ret = Z_OK;
        while (ret == Z_OK || ret == Z_BUF_ERROR) {
            zs.next_out  = out.data() + zs.total_out;
            zs.avail_out = static_cast<uInt>(out.size() - zs.total_out);
            ret = inflate(&zs, Z_SYNC_FLUSH);
            if (ret == Z_STREAM_END) break;
            if (ret == Z_BUF_ERROR && zs.avail_out == 0) {
                out.resize(out.size() * 2);
            } else if (ret < 0) {
                inflateEnd(&zs);
                return {};
            }
        }
        out.resize(zs.total_out);
        inflateEnd(&zs);
        return out;
    }

#ifdef HAVE_ZSTD
    static std::vector<uint8_t> tryInflateZstd(const std::vector<uint8_t>& data) {
        // Quick magic check: zstd magic 0xFD2FB528 (little-endian)
        if (data.size() < 4) return {};
        uint32_t magic = static_cast<uint32_t>(data[0]) |
                         (static_cast<uint32_t>(data[1]) << 8) |
                         (static_cast<uint32_t>(data[2]) << 16) |
                         (static_cast<uint32_t>(data[3]) << 24);
        if (magic != 0xFD2FB528u) return {};

        unsigned long long contentSize = ZSTD_getFrameContentSize(data.data(), data.size());
        std::vector<uint8_t> out;
        if (contentSize != ZSTD_CONTENTSIZE_UNKNOWN && contentSize != ZSTD_CONTENTSIZE_ERROR) {
            out.resize(static_cast<std::size_t>(contentSize));
            std::size_t decoded = ZSTD_decompress(out.data(), out.size(), data.data(), data.size());
            if (ZSTD_isError(decoded)) return {};
            out.resize(decoded);
        } else {
            // Unknown size: try with a heuristic buffer
            out.resize(data.size() * 10);
            std::size_t decoded = ZSTD_decompress(out.data(), out.size(), data.data(), data.size());
            if (ZSTD_isError(decoded)) {
                out.resize(data.size() * 50);
                decoded = ZSTD_decompress(out.data(), out.size(), data.data(), data.size());
                if (ZSTD_isError(decoded)) return {};
            }
            out.resize(decoded);
        }
        return out;
    }
#endif

    std::vector<uint8_t> MultiDataSource::decompress(const std::vector<uint8_t>& data) {
        // Try gzip first
        auto result = tryInflateGzip(data);
        if (!result.empty()) return result;

        // Try zstd if available
#ifdef HAVE_ZSTD
        result = tryInflateZstd(data);
        if (!result.empty()) return result;
#endif

        // Return original bytes unchanged
        return data;
    }

} // namespace routing
