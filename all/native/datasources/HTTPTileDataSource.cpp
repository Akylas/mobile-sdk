#include "HTTPTileDataSource.h"
#include "core/MapTile.h"
#include "core/BinaryData.h"
#include "components/Exceptions.h"
#include "utils/Log.h"
#include "utils/NetworkUtils.h"
#include "utils/GeneralUtils.h"

#include <cinttypes>
#include <cstring>
#include <algorithm>
#include <zlib.h>
#include <brotli/decode.h>
#ifdef HAVE_ZSTD
#include <zstd.h>
#endif

namespace carto {

    HTTPTileDataSource::HTTPTileDataSource(int minZoom, int maxZoom, const std::string& baseURL) :
        TileDataSource(minZoom, maxZoom),
        _baseURL(baseURL),
        _subdomains({ "a", "b", "c", "d" }),
        _tmsScheme(false),
        _maxAgeHeaderCheck(false),
        _timeout(-1),
        _headers(),
        _httpClient(true),
        _randomGenerator(),
        _mutex(),
        _pmtilesCache()
    {
    }
    
    HTTPTileDataSource::~HTTPTileDataSource() {
    }
    
    std::string HTTPTileDataSource::getBaseURL() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _baseURL;
    }
    
    void HTTPTileDataSource::setBaseURL(const std::string& baseURL) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _baseURL = baseURL;
            // Clear PMTiles cache when URL changes
            _pmtilesCache.reset();
        }
        notifyTilesChanged(false);
    }
    
    std::vector<std::string> HTTPTileDataSource::getSubdomains() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _subdomains;
    }
    
    void HTTPTileDataSource::setSubdomains(const std::vector<std::string>& subdomains) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _subdomains = subdomains;
        }
        notifyTilesChanged(false);
    }
    
    bool HTTPTileDataSource::isTMSScheme() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _tmsScheme;
    }
    
    void HTTPTileDataSource::setTMSScheme(bool tmsScheme) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tmsScheme = tmsScheme;
        }
        notifyTilesChanged(false);
    }
    
    bool HTTPTileDataSource::isMaxAgeHeaderCheck() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _maxAgeHeaderCheck;
    }
    
    void HTTPTileDataSource::setMaxAgeHeaderCheck(bool maxAgeCheck) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _maxAgeHeaderCheck = maxAgeCheck;
        }
        notifyTilesChanged(false);
    }
    
    int HTTPTileDataSource::getTimeout() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _timeout;
    }

    void HTTPTileDataSource::setTimeout(int timeout) {
        std::lock_guard<std::mutex> lock(_mutex);
        _httpClient.setTimeout(timeout < 0 ? -1 : timeout * 1000);
        _timeout = timeout;
    }

    std::map<std::string, std::string> HTTPTileDataSource::getHTTPHeaders() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _headers;
    }
    
    void HTTPTileDataSource::setHTTPHeaders(const std::map<std::string, std::string>& headers) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _headers = headers;
        }
        notifyTilesChanged(false);
    }
    
    std::shared_ptr<TileData> HTTPTileDataSource::loadTile(const MapTile& mapTile) {
        std::string baseURL;
        std::map<std::string, std::string> headers;
        bool maxAgeHeaderCheck;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            baseURL = _baseURL;
            headers = _headers;
            maxAgeHeaderCheck = _maxAgeHeaderCheck;
        }

        // Check if this is a PMTiles URL
        if (isPMTilesURL(baseURL)) {
            return loadPMTile(baseURL, mapTile);
        }

        std::string url = buildTileURL(baseURL, mapTile);
        if (url.empty()) {
            return std::shared_ptr<TileData>();
        }

        Log::Infof("HTTPTileDataSource::loadTile: Loading %s", url.c_str());
        std::map<std::string, std::string> responseHeaders;
        std::shared_ptr<BinaryData> responseData;
        try {
            if (_httpClient.get(url, headers, responseHeaders, responseData) != 0) {
                Log::Errorf("HTTPTileDataSource::loadTile: Failed to load %s", url.c_str());
                return std::shared_ptr<TileData>();
            }
        }
        catch (const std::exception& ex) {
            Log::Errorf("HTTPTileDataSource::loadTile: Exception while loading tile %d/%d/%d: %s", mapTile.getZoom(), mapTile.getX(), mapTile.getY(), ex.what());
            return std::shared_ptr<TileData>();
        }
        auto tileData = std::make_shared<TileData>(responseData);
        if (maxAgeHeaderCheck) {
            int maxAge = NetworkUtils::GetMaxAgeHTTPHeader(responseHeaders);
            if (maxAge >= 0) {
                tileData->setMaxAge(maxAge * 1000);
            }
        }
        return tileData;
    }
    
    std::string HTTPTileDataSource::buildTileURL(const std::string& baseURL, const MapTile& tile) const {
        bool tmsScheme = false;
        std::string subdomain;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            tmsScheme = _tmsScheme;
            if (!_subdomains.empty()) {
                std::size_t randomIndex = std::uniform_int_distribution<std::size_t>(0, _subdomains.size() - 1)(_randomGenerator);
                subdomain = _subdomains[randomIndex];
            }
        }

        std::map<std::string, std::string> tagValues = buildTagValues(tmsScheme ? tile.getFlipped() : tile);
        if (!subdomain.empty()) {
            tagValues["s"] = subdomain;
        }
   
        return GeneralUtils::ReplaceTags(baseURL, tagValues, "{", "}", true);
    }
    
    // PMTiles support implementation
    
    bool HTTPTileDataSource::isPMTilesURL(const std::string& url) const {
        // Check if URL starts with "pmtiles://"
        if (url.size() >= 11 && url.substr(0, 11) == "pmtiles://") {
            return true;
        }
        
        // Check if URL ends with ".pmtiles"
        if (url.size() >= 8) {
            std::string extension = url.substr(url.size() - 8);
            if (extension == ".pmtiles") {
                return true;
            }
        }
        
        return false;
    }
    
    std::string HTTPTileDataSource::normalizePMTilesURL(const std::string& url) const {
        // If URL starts with "pmtiles://", replace with "https://"
        if (url.size() >= 11 && url.substr(0, 11) == "pmtiles://") {
            return "https://" + url.substr(11);
        }
        return url;
    }
    
    std::shared_ptr<TileData> HTTPTileDataSource::loadPMTile(const std::string& baseURL, const MapTile& mapTile) {
        try {
            std::lock_guard<std::mutex> lock(_mutex);
            
            std::string url = normalizePMTilesURL(baseURL);
            
            // Initialize PMTiles cache if needed
            if (!_pmtilesCache || _pmtilesCache->url != url) {
                Log::Infof("HTTPTileDataSource::loadPMTile: Initializing PMTiles archive from %s", url.c_str());
                
                _pmtilesCache = std::make_unique<PMTilesCache>();
                _pmtilesCache->url = url;
                _pmtilesCache->header = readPMTilesHeader(url);
                
                // Read and decode root directory
                std::vector<uint8_t> rootDirData = httpRangeRequest(url, _pmtilesCache->header.rootDirectoryOffset, _pmtilesCache->header.rootDirectoryLength);
                std::vector<uint8_t> decompressed = decompressPMTilesData(rootDirData, _pmtilesCache->header.internalCompression);
                _pmtilesCache->rootDirectory = decodePMTilesDirectory(decompressed);
                
                Log::Infof("HTTPTileDataSource::loadPMTile: PMTiles header loaded, tiles: %" PRIu64 ", zoom: %d-%d", 
                           _pmtilesCache->header.numTileEntries, _pmtilesCache->header.minZoom, _pmtilesCache->header.maxZoom);
            }
            
            // Convert tile coordinates to PMTiles TileID
            uint64_t tileId = zxyToTileId(mapTile.getZoom(), mapTile.getX(), mapTile.getY());
            
            // Search for tile in root directory
            PMTilesDirectoryEntry entry;
            bool found = findPMTileEntry(_pmtilesCache->rootDirectory, tileId, entry);
            
            // If not found in root, search in leaf directories
            if (!found) {
                // Binary search in root directory to find the leaf directory
                auto it = std::lower_bound(_pmtilesCache->rootDirectory.begin(), _pmtilesCache->rootDirectory.end(), tileId,
                    [](const PMTilesDirectoryEntry& entry, uint64_t id) { return entry.tileId < id; });
                
                if (it != _pmtilesCache->rootDirectory.begin()) {
                    --it;
                    
                    // Check if this points to a leaf directory
                    if (it->runLength == 0) {
                        // Load leaf directory (with caching)
                        uint64_t leafKey = it->offset;
                        auto leafIt = _pmtilesCache->leafDirectoryCache.find(leafKey);
                        if (leafIt == _pmtilesCache->leafDirectoryCache.end()) {
                            std::vector<PMTilesDirectoryEntry> leafDir = loadPMTilesLeafDirectory(url, _pmtilesCache->header, it->offset, it->length);
                            leafIt = _pmtilesCache->leafDirectoryCache.insert({leafKey, std::move(leafDir)}).first;
                        }
                        
                        found = findPMTileEntry(leafIt->second, tileId, entry);
                    }
                }
            }
            
            if (!found) {
                // Tile not found, try parent tile
                if (mapTile.getZoom() > getMinZoom()) {
                    Log::Infof("HTTPTileDataSource::loadPMTile: Tile not found, redirecting to parent");
                    auto tileData = std::make_shared<TileData>(std::shared_ptr<BinaryData>());
                    tileData->setReplaceWithParent(true);
                    return tileData;
                } else {
                    Log::Infof("HTTPTileDataSource::loadPMTile: Tile not found");
                    return std::shared_ptr<TileData>();
                }
            }
            
            // Read tile data
            std::vector<uint8_t> compressedData = httpRangeRequest(url, 
                _pmtilesCache->header.tileDataOffset + entry.offset, entry.length);
            
            // Decompress if needed
            std::vector<uint8_t> tileBytes = decompressPMTilesData(compressedData, _pmtilesCache->header.tileCompression);
            
            auto data = std::make_shared<BinaryData>(tileBytes.data(), tileBytes.size());
            return std::make_shared<TileData>(data);
        }
        catch (const std::exception& ex) {
            Log::Errorf("HTTPTileDataSource::loadPMTile: Failed to load tile: %s", ex.what());
            return std::shared_ptr<TileData>();
        }
    }
    
    HTTPTileDataSource::PMTilesHeader HTTPTileDataSource::readPMTilesHeader(const std::string& url) {
        // Read 127-byte header
        std::vector<uint8_t> headerBytes = httpRangeRequest(url, 0, 127);
        
        if (headerBytes.size() != 127) {
            throw GenericException("Failed to read PMTiles header");
        }
        
        // Check magic number "PMTiles"
        if (std::memcmp(headerBytes.data(), "PMTiles", 7) != 0) {
            throw GenericException("Invalid PMTiles magic number");
        }
        
        // Check version (must be 3)
        if (headerBytes[7] != 3) {
            throw GenericException("Unsupported PMTiles version (only v3 is supported)");
        }
        
        PMTilesHeader header;
        
        // Helper lambda to read little-endian uint64
        auto readUInt64 = [](const uint8_t* data) -> uint64_t {
            return static_cast<uint64_t>(data[0]) |
                   (static_cast<uint64_t>(data[1]) << 8) |
                   (static_cast<uint64_t>(data[2]) << 16) |
                   (static_cast<uint64_t>(data[3]) << 24) |
                   (static_cast<uint64_t>(data[4]) << 32) |
                   (static_cast<uint64_t>(data[5]) << 40) |
                   (static_cast<uint64_t>(data[6]) << 48) |
                   (static_cast<uint64_t>(data[7]) << 56);
        };
        
        // Helper lambda to read little-endian int32
        auto readInt32 = [](const uint8_t* data) -> int32_t {
            return static_cast<int32_t>(
                static_cast<uint32_t>(data[0]) |
                (static_cast<uint32_t>(data[1]) << 8) |
                (static_cast<uint32_t>(data[2]) << 16) |
                (static_cast<uint32_t>(data[3]) << 24)
            );
        };
        
        header.rootDirectoryOffset = readUInt64(&headerBytes[8]);
        header.rootDirectoryLength = readUInt64(&headerBytes[16]);
        header.metadataOffset = readUInt64(&headerBytes[24]);
        header.metadataLength = readUInt64(&headerBytes[32]);
        header.leafDirectoriesOffset = readUInt64(&headerBytes[40]);
        header.leafDirectoriesLength = readUInt64(&headerBytes[48]);
        header.tileDataOffset = readUInt64(&headerBytes[56]);
        header.tileDataLength = readUInt64(&headerBytes[64]);
        header.numAddressedTiles = readUInt64(&headerBytes[72]);
        header.numTileEntries = readUInt64(&headerBytes[80]);
        header.numTileContents = readUInt64(&headerBytes[88]);
        header.clustered = headerBytes[96];
        header.internalCompression = headerBytes[97];
        header.tileCompression = headerBytes[98];
        header.tileType = headerBytes[99];
        header.minZoom = headerBytes[100];
        header.maxZoom = headerBytes[101];
        
        // Read positions (4 bytes for longitude, 4 bytes for latitude = 8 bytes per position)
        int32_t minLonE7 = readInt32(&headerBytes[102]);
        int32_t minLatE7 = readInt32(&headerBytes[106]);
        int32_t maxLonE7 = readInt32(&headerBytes[110]);
        int32_t maxLatE7 = readInt32(&headerBytes[114]);
        
        header.minLon = minLonE7 / 10000000.0;
        header.minLat = minLatE7 / 10000000.0;
        header.maxLon = maxLonE7 / 10000000.0;
        header.maxLat = maxLatE7 / 10000000.0;
        
        header.centerZoom = headerBytes[118];
        int32_t centerLonE7 = readInt32(&headerBytes[119]);
        int32_t centerLatE7 = readInt32(&headerBytes[123]);
        header.centerLon = centerLonE7 / 10000000.0;
        header.centerLat = centerLatE7 / 10000000.0;
        
        return header;
    }
    
    std::vector<uint8_t> HTTPTileDataSource::httpRangeRequest(const std::string& url, uint64_t offset, uint64_t length) {
        std::map<std::string, std::string> requestHeaders = _headers;
        requestHeaders["Range"] = "bytes=" + std::to_string(offset) + "-" + std::to_string(offset + length - 1);
        
        std::map<std::string, std::string> responseHeaders;
        std::shared_ptr<BinaryData> responseData;
        
        if (_httpClient.get(url, requestHeaders, responseHeaders, responseData) != 0) {
            throw GenericException("HTTP range request failed");
        }
        
        if (!responseData || responseData->size() == 0) {
            throw GenericException("Empty HTTP range response");
        }
        
        return std::vector<uint8_t>(responseData->data(), responseData->data() + responseData->size());
    }
    
    std::vector<uint8_t> HTTPTileDataSource::decompressPMTilesData(const std::vector<uint8_t>& data, uint8_t compression) {
        // Compression: 0x00=Unknown, 0x01=None, 0x02=gzip, 0x03=brotli, 0x04=zstd
        
        if (compression == 0x01 || compression == 0x00) {
            // No compression or unknown (treat as uncompressed)
            return data;
        }
        else if (compression == 0x02) {
            // gzip decompression
            z_stream stream;
            std::memset(&stream, 0, sizeof(stream));
            
            // Initialize for gzip (windowBits = 15 + 16 for gzip)
            if (inflateInit2(&stream, 15 + 16) != Z_OK) {
                throw GenericException("Failed to initialize gzip decompression");
            }
            
            stream.avail_in = data.size();
            stream.next_in = const_cast<uint8_t*>(data.data());
            
            std::vector<uint8_t> result;
            result.reserve(data.size() * 4); // Estimate 4x compression
            
            uint8_t buffer[32768];
            int ret;
            
            do {
                stream.avail_out = sizeof(buffer);
                stream.next_out = buffer;
                
                ret = inflate(&stream, Z_NO_FLUSH);
                
                if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                    inflateEnd(&stream);
                    throw GenericException("gzip decompression failed");
                }
                
                size_t have = sizeof(buffer) - stream.avail_out;
                result.insert(result.end(), buffer, buffer + have);
                
            } while (ret != Z_STREAM_END);
            
            inflateEnd(&stream);
            return result;
        }
        else if (compression == 0x03) {
            // Brotli decompression
            // Initial buffer size: estimate 10x compression ratio (typical for map tiles)
            size_t maxOutputSize = data.size() * 10;
            std::vector<uint8_t> result(maxOutputSize);
            size_t decodedSize = maxOutputSize;
            
            BrotliDecoderResult status = BrotliDecoderDecompress(
                data.size(),
                data.data(),
                &decodedSize,
                result.data()
            );
            
            // If buffer was too small, retry with larger buffer
            // Fallback buffer size: 50x for edge cases with high compression ratios
            if (status == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
                maxOutputSize = data.size() * 50;
                result.resize(maxOutputSize);
                decodedSize = maxOutputSize;
                
                status = BrotliDecoderDecompress(
                    data.size(),
                    data.data(),
                    &decodedSize,
                    result.data()
                );
            }
            
            if (status != BROTLI_DECODER_RESULT_SUCCESS) {
                Log::Errorf("HTTPTileDataSource: Brotli decompression failed with status code %d", static_cast<int>(status));
                throw GenericException("Brotli decompression failed");
            }
            
            result.resize(decodedSize);
            return result;
        }
#ifdef HAVE_ZSTD
        else if (compression == 0x04) {
            // Zstandard decompression
            unsigned long long const decompressedSize = ZSTD_getFrameContentSize(data.data(), data.size());
            
            if (decompressedSize == ZSTD_CONTENTSIZE_ERROR) {
                Log::Errorf("HTTPTileDataSource: Invalid zstd compressed data");
                throw GenericException("Invalid zstd compressed data");
            }
            else if (decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
                // Size unknown, use heuristic
                // Initial buffer size: estimate 10x compression ratio (typical for map tiles)
                size_t maxOutputSize = data.size() * 10;
                std::vector<uint8_t> result(maxOutputSize);
                
                size_t actualSize = ZSTD_decompress(result.data(), maxOutputSize, data.data(), data.size());
                
                if (ZSTD_isError(actualSize)) {
                    // Try with larger buffer
                    // Fallback buffer size: 50x for edge cases with high compression ratios
                    maxOutputSize = data.size() * 50;
                    result.resize(maxOutputSize);
                    actualSize = ZSTD_decompress(result.data(), maxOutputSize, data.data(), data.size());
                    
                    if (ZSTD_isError(actualSize)) {
                        Log::Errorf("HTTPTileDataSource: Zstandard decompression failed: %s", ZSTD_getErrorName(actualSize));
                        throw GenericException("Zstandard decompression failed");
                    }
                }
                
                result.resize(actualSize);
                return result;
            }
            else {
                // Size is known
                std::vector<uint8_t> result(decompressedSize);
                
                size_t actualSize = ZSTD_decompress(result.data(), decompressedSize, data.data(), data.size());
                
                if (ZSTD_isError(actualSize)) {
                    Log::Errorf("HTTPTileDataSource: Zstandard decompression failed: %s", ZSTD_getErrorName(actualSize));
                    throw GenericException("Zstandard decompression failed");
                }
                
                return result;
            }
        }
#endif
        else {
            Log::Errorf("HTTPTileDataSource: Unknown compression type: %d", compression);
            throw GenericException("Unknown compression type");
        }
    }
    
    std::vector<HTTPTileDataSource::PMTilesDirectoryEntry> HTTPTileDataSource::decodePMTilesDirectory(const std::vector<uint8_t>& data) {
        size_t offset = 0;
        
        // Read number of entries
        uint64_t numEntries = readPMTilesVarint(data, offset);
        
        if (numEntries == 0) {
            return std::vector<PMTilesDirectoryEntry>();
        }
        
        std::vector<PMTilesDirectoryEntry> entries(numEntries);
        
        // Read TileIDs (delta-encoded)
        uint64_t lastId = 0;
        for (uint64_t i = 0; i < numEntries; i++) {
            uint64_t delta = readPMTilesVarint(data, offset);
            lastId += delta;
            entries[i].tileId = lastId;
        }
        
        // Read RunLengths
        for (uint64_t i = 0; i < numEntries; i++) {
            entries[i].runLength = static_cast<uint32_t>(readPMTilesVarint(data, offset));
        }
        
        // Read Lengths
        for (uint64_t i = 0; i < numEntries; i++) {
            entries[i].length = static_cast<uint32_t>(readPMTilesVarint(data, offset));
        }
        
        // Read Offsets
        for (uint64_t i = 0; i < numEntries; i++) {
            uint64_t value = readPMTilesVarint(data, offset);
            
            if (value == 0 && i > 0) {
                // Offset is contiguous with previous entry
                entries[i].offset = entries[i - 1].offset + entries[i - 1].length;
            } else {
                entries[i].offset = value - 1;
            }
        }
        
        return entries;
    }
    
    uint64_t HTTPTileDataSource::readPMTilesVarint(const std::vector<uint8_t>& data, size_t& offset) {
        uint64_t result = 0;
        int shift = 0;
        
        while (offset < data.size()) {
            uint8_t byte = data[offset++];
            result |= static_cast<uint64_t>(byte & 0x7F) << shift;
            
            if ((byte & 0x80) == 0) {
                break;
            }
            
            shift += 7;
        }
        
        return result;
    }
    
    uint64_t HTTPTileDataSource::zxyToTileId(int z, int x, int y) {
        if (z == 0) {
            return 0;
        }
        
        // Calculate the base TileID for this zoom level
        uint64_t baseTileId = 0;
        for (int i = 0; i < z; i++) {
            baseTileId += (1ULL << (i * 2));
        }
        
        // Hilbert curve encoding for spatial indexing
        // This lambda performs quadrant rotation during Hilbert curve traversal
        auto rotateQuadrant = [](int n, int* x, int* y, int rx, int ry) {
            if (ry == 0) {
                if (rx == 1) {
                    *x = n - 1 - *x;
                    *y = n - 1 - *y;
                }
                int t = *x;
                *x = *y;
                *y = t;
            }
        };
        
        int n = 1 << z;
        int rx, ry, s;
        int tx = x;
        int ty = y;
        uint64_t d = 0;
        
        for (s = n / 2; s > 0; s /= 2) {
            rx = (tx & s) > 0 ? 1 : 0;
            ry = (ty & s) > 0 ? 1 : 0;
            // Standard Hilbert curve quadrant encoding formula
            d += s * s * ((3 * rx) ^ ry);
            rotateQuadrant(s, &tx, &ty, rx, ry);
        }
        
        return baseTileId + d;
    }
    
    bool HTTPTileDataSource::findPMTileEntry(const std::vector<PMTilesDirectoryEntry>& directory, uint64_t tileId, PMTilesDirectoryEntry& outEntry) {
        for (const auto& entry : directory) {
            if (entry.runLength == 0) {
                // This is a pointer to a leaf directory, not a tile
                continue;
            }
            
            if (tileId >= entry.tileId && tileId < entry.tileId + entry.runLength) {
                outEntry = entry;
                return true;
            }
        }
        return false;
    }
    
    std::vector<HTTPTileDataSource::PMTilesDirectoryEntry> HTTPTileDataSource::loadPMTilesLeafDirectory(const std::string& url, const PMTilesHeader& header, uint64_t offset, uint32_t length) {
        std::vector<uint8_t> leafDirData = httpRangeRequest(url, header.leafDirectoriesOffset + offset, length);
        std::vector<uint8_t> decompressed = decompressPMTilesData(leafDirData, header.internalCompression);
        return decodePMTilesDirectory(decompressed);
    }
    
}
