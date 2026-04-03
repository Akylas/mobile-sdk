/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_PMTILESUTILS_H_
#define _CARTO_PMTILESUTILS_H_

#include <cstdint>
#include <vector>

namespace carto {
namespace pmtiles {

    /**
     * PMTiles v3 archive header structure.
     * Contains metadata about the archive including directory locations,
     * compression settings, and spatial bounds.
     */
    struct Header {
        uint64_t rootDirectoryOffset;
        uint64_t rootDirectoryLength;
        uint64_t metadataOffset;
        uint64_t metadataLength;
        uint64_t leafDirectoriesOffset;
        uint64_t leafDirectoriesLength;
        uint64_t tileDataOffset;
        uint64_t tileDataLength;
        uint64_t numAddressedTiles;
        uint64_t numTileEntries;
        uint64_t numTileContents;
        uint8_t clustered;
        uint8_t internalCompression;
        uint8_t tileCompression;
        uint8_t tileType;
        uint8_t minZoom;
        uint8_t maxZoom;
        double minLon;
        double minLat;
        double maxLon;
        double maxLat;
        uint8_t centerZoom;
        double centerLon;
        double centerLat;
    };

    /**
     * Directory entry structure for PMTiles.
     * Each entry represents either a tile or a pointer to a leaf directory.
     */
    struct DirectoryEntry {
        uint64_t tileId;
        uint64_t offset;
        uint32_t length;
        uint32_t runLength;
    };

    /**
     * Read a PMTiles header from raw bytes.
     * @param headerBytes Pointer to 127 bytes of header data
     * @return Parsed header structure
     * @throws GenericException if the header is invalid or unsupported
     */
    Header readHeader(const uint8_t* headerBytes);

    /**
     * Decompress PMTiles data based on the compression type.
     * @param data Compressed data
     * @param compression Compression type (0x00=Unknown, 0x01=None, 0x02=gzip, 0x03=brotli, 0x04=zstd)
     * @return Decompressed data
     * @throws GenericException if decompression fails
     */
    std::vector<uint8_t> decompressData(const std::vector<uint8_t>& data, uint8_t compression);

    /**
     * Decode a PMTiles directory from decompressed data.
     * @param data Decompressed directory data
     * @return Vector of directory entries
     */
    std::vector<DirectoryEntry> decodeDirectory(const std::vector<uint8_t>& data);

    /**
     * Read a varint from a byte array.
     * @param data Byte array to read from
     * @param offset Current offset (will be updated to point after the varint)
     * @return Decoded varint value
     */
    uint64_t readVarint(const std::vector<uint8_t>& data, size_t& offset);

    /**
     * Convert tile coordinates (z, x, y) to PMTiles TileID using Hilbert curve.
     * @param z Zoom level
     * @param x Tile X coordinate
     * @param y Tile Y coordinate
     * @return TileID
     */
    uint64_t zxyToTileId(int z, int x, int y);

    /**
     * Convert PMTiles TileID to tile coordinates (z, x, y) using inverse Hilbert curve.
     * @param tileId TileID to convert
     * @param z Output zoom level
     * @param x Output tile X coordinate
     * @param y Output tile Y coordinate
     */
    void tileIdToZxy(uint64_t tileId, int& z, int& x, int& y);

    /**
     * Find a tile entry in a directory.
     * @param directory Directory entries to search
     * @param tileId TileID to find
     * @param outEntry Output entry if found
     * @return true if found, false otherwise
     */
    bool findTileEntry(const std::vector<DirectoryEntry>& directory, uint64_t tileId, DirectoryEntry& outEntry);

} // namespace pmtiles
} // namespace carto

#endif
