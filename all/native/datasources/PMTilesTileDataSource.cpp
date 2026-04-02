#ifdef _CARTO_OFFLINE_SUPPORT

#include "PMTilesTileDataSource.h"
#include "core/BinaryData.h"
#include "core/MapTile.h"
#include "components/Exceptions.h"
#include "projections/Projection.h"
#include "utils/Log.h"

#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <zlib.h>

// For JSON parsing (simple extraction)
#include <boost/algorithm/string.hpp>

namespace carto {

    PMTilesTileDataSource::PMTilesTileDataSource(const std::string& path) :
        TileDataSource(),
        _path(path),
        _file(),
        _header(),
        _rootDirectory(),
        _cachedMetadata(),
        _cachedDataExtent(),
        _leafDirectoryCache(),
        _mutex()
    {
        _file = std::make_unique<std::ifstream>(path, std::ios::binary);
        if (!_file->is_open()) {
            throw FileException("Failed to open PMTiles file", path);
        }

        _header = ReadHeader(*_file);
        
        // Read and decode root directory
        std::vector<uint8_t> rootDirData(_header.rootDirectoryLength);
        _file->seekg(_header.rootDirectoryOffset);
        _file->read(reinterpret_cast<char*>(rootDirData.data()), _header.rootDirectoryLength);
        
        std::vector<uint8_t> decompressed = DecompressData(rootDirData, _header.internalCompression);
        _rootDirectory = DecodeDirectory(decompressed);
        
        Log::Infof("PMTilesTileDataSource: Opened %s with %llu tiles, zoom %d-%d", 
                   path.c_str(), _header.numTileEntries, _header.minZoom, _header.maxZoom);
    }

    PMTilesTileDataSource::PMTilesTileDataSource(int minZoom, int maxZoom, const std::string& path) :
        TileDataSource(minZoom, maxZoom),
        _path(path),
        _file(),
        _header(),
        _rootDirectory(),
        _cachedMetadata(),
        _cachedDataExtent(),
        _leafDirectoryCache(),
        _mutex()
    {
        _file = std::make_unique<std::ifstream>(path, std::ios::binary);
        if (!_file->is_open()) {
            throw FileException("Failed to open PMTiles file", path);
        }

        _header = ReadHeader(*_file);
        
        // Read and decode root directory
        std::vector<uint8_t> rootDirData(_header.rootDirectoryLength);
        _file->seekg(_header.rootDirectoryOffset);
        _file->read(reinterpret_cast<char*>(rootDirData.data()), _header.rootDirectoryLength);
        
        std::vector<uint8_t> decompressed = DecompressData(rootDirData, _header.internalCompression);
        _rootDirectory = DecodeDirectory(decompressed);
        
        Log::Infof("PMTilesTileDataSource: Opened %s with %llu tiles, zoom %d-%d", 
                   path.c_str(), _header.numTileEntries, _header.minZoom, _header.maxZoom);
    }
        
    PMTilesTileDataSource::~PMTilesTileDataSource() {
    }
    
    std::string PMTilesTileDataSource::getMetaData() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        
        if (!_cachedMetadata) {
            if (_header.metadataLength == 0) {
                _cachedMetadata = "";
                return *_cachedMetadata;
            }
            
            try {
                // Read metadata
                std::vector<uint8_t> metadataData(_header.metadataLength);
                _file->seekg(_header.metadataOffset);
                _file->read(reinterpret_cast<char*>(metadataData.data()), _header.metadataLength);
                
                // Decompress if needed
                std::vector<uint8_t> decompressed = DecompressData(metadataData, _header.internalCompression);
                
                _cachedMetadata = std::string(decompressed.begin(), decompressed.end());
            }
            catch (const std::exception& ex) {
                Log::Errorf("PMTilesTileDataSource::getMetaData: Failed to read metadata: %s", ex.what());
                _cachedMetadata = "";
            }
        }
        
        return *_cachedMetadata;
    }

    std::string PMTilesTileDataSource::getMetaData(const std::string& key) const {
        std::string metadata = getMetaData();
        if (metadata.empty()) {
            return "";
        }
        
        // Simple JSON key extraction (not a full JSON parser)
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = metadata.find(searchKey);
        if (keyPos == std::string::npos) {
            return "";
        }
        
        // Find the colon after the key
        size_t colonPos = metadata.find(':', keyPos);
        if (colonPos == std::string::npos) {
            return "";
        }
        
        // Skip whitespace after colon
        size_t valueStart = metadata.find_first_not_of(" \t\n\r", colonPos + 1);
        if (valueStart == std::string::npos) {
            return "";
        }
        
        // Extract value (handle strings and non-strings)
        if (metadata[valueStart] == '"') {
            // String value
            size_t valueEnd = metadata.find('"', valueStart + 1);
            if (valueEnd != std::string::npos) {
                return metadata.substr(valueStart + 1, valueEnd - valueStart - 1);
            }
        } else {
            // Non-string value (number, boolean, etc.)
            size_t valueEnd = metadata.find_first_of(",}", valueStart);
            if (valueEnd != std::string::npos) {
                std::string value = metadata.substr(valueStart, valueEnd - valueStart);
                boost::trim(value);
                return value;
            }
        }
        
        return "";
    }

    int PMTilesTileDataSource::getMinZoom() const {
        return _header.minZoom;
    }
    
    int PMTilesTileDataSource::getMaxZoom() const {
        return _header.maxZoom;
    }
    
    MapBounds PMTilesTileDataSource::getDataExtent() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        
        if (!_cachedDataExtent) {
            // Convert WGS84 bounds to projection bounds
            MapPos minPos = _projection->fromWgs84(MapPos(_header.minLon, _header.minLat));
            MapPos maxPos = _projection->fromWgs84(MapPos(_header.maxLon, _header.maxLat));
            _cachedDataExtent = MapBounds(minPos, maxPos);
        }
        
        return *_cachedDataExtent;
    }

    std::shared_ptr<TileData> PMTilesTileDataSource::loadTile(const MapTile& mapTile) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        
        Log::Infof("PMTilesTileDataSource::loadTile: Loading %s", mapTile.toString().c_str());
        
        if (!_file || !_file->is_open()) {
            Log::Errorf("PMTilesTileDataSource::loadTile: File not open");
            return std::shared_ptr<TileData>();
        }

        // Handle overzoom
        if (getMaxOverzoomLevel() >= 0 && mapTile.getZoom() > getMaxZoomWithOverzoom()) {
            auto tileData = std::make_shared<TileData>(std::shared_ptr<BinaryData>());
            tileData->setIsOverZoom(true);
            return tileData;
        }
        
        try {
            // Convert tile coordinates to TileID using Hilbert curve
            uint64_t tileId = ZxyToTileId(mapTile.getZoom(), mapTile.getX(), mapTile.getY());
            
            // Find the tile entry
            DirectoryEntry entry;
            if (!FindTileEntry(tileId, entry)) {
                // Tile not found, try parent tile
                if (mapTile.getZoom() > getMinZoom()) {
                    Log::Infof("PMTilesTileDataSource::loadTile: Tile not found, redirecting to parent");
                    std::shared_ptr<TileData> tileData = std::make_shared<TileData>(std::shared_ptr<BinaryData>());
                    tileData->setReplaceWithParent(true);
                    return tileData;
                } else {
                    Log::Infof("PMTilesTileDataSource::loadTile: Tile not found");
                    return std::shared_ptr<TileData>();
                }
            }
            
            // Read tile data
            std::vector<uint8_t> compressedData = ReadTileData(entry.offset, entry.length);
            
            // Decompress if needed
            std::vector<uint8_t> tileBytes = DecompressData(compressedData, _header.tileCompression);
            
            auto data = std::make_shared<BinaryData>(tileBytes.data(), tileBytes.size());
            return std::make_shared<TileData>(data);
        }
        catch (const std::exception& ex) {
            Log::Errorf("PMTilesTileDataSource::loadTile: Failed to load tile: %s", ex.what());
            return std::shared_ptr<TileData>();
        }
    }

    PMTilesTileDataSource::PMTilesHeader PMTilesTileDataSource::ReadHeader(std::ifstream& file) {
        // Read 127-byte header
        uint8_t headerBytes[127];
        file.seekg(0);
        file.read(reinterpret_cast<char*>(headerBytes), 127);
        
        if (!file) {
            throw GenericException("Failed to read PMTiles header");
        }
        
        // Check magic number "PMTiles"
        if (std::memcmp(headerBytes, "PMTiles", 7) != 0) {
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
        
        // Read positions (8 bytes each)
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

    std::vector<uint8_t> PMTilesTileDataSource::DecompressData(const std::vector<uint8_t>& data, uint8_t compression) {
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
            // TODO: Add brotli support when library is available
            // #include <brotli/decode.h>
            // Use BrotliDecoderDecompress() for decompression
            Log::Error("PMTilesTileDataSource: Brotli compression not yet supported. Please use gzip or uncompressed PMTiles files.");
            throw GenericException("Brotli compression not yet supported");
        }
        else if (compression == 0x04) {
            // Zstandard decompression
            // TODO: Add zstd support when library is available
            // #include <zstd.h>
            // Use ZSTD_decompress() for decompression
            Log::Error("PMTilesTileDataSource: Zstandard compression not yet supported. Please use gzip or uncompressed PMTiles files.");
            throw GenericException("Zstandard compression not yet supported");
        }
        else {
            Log::Errorf("PMTilesTileDataSource: Unknown compression type: %d", compression);
            throw GenericException("Unknown compression type");
        }
    }

    std::vector<PMTilesTileDataSource::DirectoryEntry> PMTilesTileDataSource::DecodeDirectory(const std::vector<uint8_t>& data) {
        size_t offset = 0;
        
        // Read number of entries
        uint64_t numEntries = ReadVarint(data, offset);
        
        if (numEntries == 0) {
            return std::vector<DirectoryEntry>();
        }
        
        std::vector<DirectoryEntry> entries(numEntries);
        
        // Read TileIDs (delta-encoded)
        uint64_t lastId = 0;
        for (uint64_t i = 0; i < numEntries; i++) {
            uint64_t delta = ReadVarint(data, offset);
            lastId += delta;
            entries[i].tileId = lastId;
        }
        
        // Read RunLengths
        for (uint64_t i = 0; i < numEntries; i++) {
            entries[i].runLength = static_cast<uint32_t>(ReadVarint(data, offset));
        }
        
        // Read Lengths
        for (uint64_t i = 0; i < numEntries; i++) {
            entries[i].length = static_cast<uint32_t>(ReadVarint(data, offset));
        }
        
        // Read Offsets
        for (uint64_t i = 0; i < numEntries; i++) {
            uint64_t value = ReadVarint(data, offset);
            
            if (value == 0 && i > 0) {
                // Offset is contiguous with previous entry
                entries[i].offset = entries[i - 1].offset + entries[i - 1].length;
            } else {
                entries[i].offset = value - 1;
            }
        }
        
        return entries;
    }

    uint64_t PMTilesTileDataSource::ReadVarint(const std::vector<uint8_t>& data, size_t& offset) {
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

    uint64_t PMTilesTileDataSource::ZxyToTileId(int z, int x, int y) {
        if (z == 0) {
            return 0;
        }
        
        // Calculate the base TileID for this zoom level
        // TileID = sum of all tiles at all previous zoom levels
        uint64_t baseTileId = 0;
        for (int i = 0; i < z; i++) {
            baseTileId += (1ULL << (i * 2));
        }
        
        // Hilbert curve encoding
        // Based on the PMTiles spec which uses Hilbert curves
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
            d += s * s * ((3 * rx) ^ ry);
            rotateQuadrant(s, &tx, &ty, rx, ry);
        }
        
        return baseTileId + d;
    }

    void PMTilesTileDataSource::TileIdToZxy(uint64_t tileId, int& z, int& x, int& y) {
        if (tileId == 0) {
            z = 0;
            x = 0;
            y = 0;
            return;
        }
        
        // Find zoom level
        uint64_t acc = 0;
        z = 0;
        while (acc + (1ULL << (z * 2)) <= tileId) {
            acc += (1ULL << (z * 2));
            z++;
        }
        
        // Get position within zoom level
        uint64_t d = tileId - acc;
        
        // Inverse Hilbert curve
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
        int tx = 0;
        int ty = 0;
        uint64_t t = d;
        
        for (s = 1; s < n; s *= 2) {
            rx = 1 & (t / 2);
            ry = 1 & (t ^ rx);
            rotateQuadrant(s, &tx, &ty, rx, ry);
            tx += s * rx;
            ty += s * ry;
            t /= 4;
        }
        
        x = tx;
        y = ty;
    }

    std::vector<uint8_t> PMTilesTileDataSource::ReadTileData(uint64_t offset, uint32_t length) {
        std::vector<uint8_t> data(length);
        _file->seekg(_header.tileDataOffset + offset);
        _file->read(reinterpret_cast<char*>(data.data()), length);
        
        if (!_file) {
            throw GenericException("Failed to read tile data");
        }
        
        return data;
    }

    bool PMTilesTileDataSource::FindTileEntry(uint64_t tileId, DirectoryEntry& outEntry) {
        // Search in root directory first
        for (const auto& entry : _rootDirectory) {
            if (entry.runLength == 0) {
                // This is a leaf directory entry
                if (entry.tileId <= tileId) {
                    // Check if this leaf directory might contain our tile
                    try {
                        // Check cache first
                        auto cacheIt = _leafDirectoryCache.find(entry.offset);
                        std::vector<DirectoryEntry>* leafDir;
                        
                        if (cacheIt != _leafDirectoryCache.end()) {
                            leafDir = &cacheIt->second;
                        } else {
                            // Load and cache the leaf directory
                            std::vector<DirectoryEntry> loadedLeafDir = LoadLeafDirectory(entry.offset, entry.length);
                            auto insertResult = _leafDirectoryCache.emplace(entry.offset, std::move(loadedLeafDir));
                            leafDir = &insertResult.first->second;
                        }
                        
                        // Search in the leaf directory
                        for (const auto& leafEntry : *leafDir) {
                            if (leafEntry.runLength > 0 && 
                                tileId >= leafEntry.tileId && 
                                tileId < leafEntry.tileId + leafEntry.runLength) {
                                outEntry = leafEntry; // Copy the entry
                                return true;
                            }
                        }
                    }
                    catch (const std::exception& ex) {
                        Log::Warnf("PMTilesTileDataSource: Failed to load leaf directory: %s", ex.what());
                    }
                }
            } else {
                // This is a tile entry
                if (tileId >= entry.tileId && tileId < entry.tileId + entry.runLength) {
                    outEntry = entry; // Copy the entry
                    return true;
                }
            }
        }
        
        return false;
    }

    std::vector<PMTilesTileDataSource::DirectoryEntry> PMTilesTileDataSource::LoadLeafDirectory(uint64_t offset, uint32_t length) {
        // Read leaf directory data
        std::vector<uint8_t> leafData(length);
        _file->seekg(_header.leafDirectoriesOffset + offset);
        _file->read(reinterpret_cast<char*>(leafData.data()), length);
        
        if (!_file) {
            throw GenericException("Failed to read leaf directory");
        }
        
        // Decompress and decode
        std::vector<uint8_t> decompressed = DecompressData(leafData, _header.internalCompression);
        return DecodeDirectory(decompressed);
    }

}

#endif
