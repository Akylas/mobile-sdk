/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_PMTILESTILEDATASOURCE_H_
#define _CARTO_PMTILESTILEDATASOURCE_H_

#ifdef _CARTO_OFFLINE_SUPPORT

#include "datasources/TileDataSource.h"

#include <map>
#include <memory>
#include <optional>
#include <vector>
#include <fstream>

namespace carto {
    
    /**
     * A tile data source that loads tiles from a PMTiles v3 archive file.
     * PMTiles is a single-file archive format for pyramids of tiled data.
     * The archive format supports efficient random access and metadata storage.
     */
    class PMTilesTileDataSource : public TileDataSource {
    public:
        /**
         * Constructs a PMTilesTileDataSource object.
         * Min and max zoom levels are automatically detected from the PMTiles header.
         * @param path The path to the PMTiles archive file.
         * @throws std::exception If the file could not be opened or is not a valid PMTiles archive.
         */
        explicit PMTilesTileDataSource(const std::string& path);
        
        /**
         * Constructs a PMTilesTileDataSource object with specified zoom levels.
         * @param minZoom The minimum zoom level supported by this data source.
         * @param maxZoom The maximum zoom level supported by this data source.
         * @param path The path to the PMTiles archive file.
         * @throws std::exception If the file could not be opened or is not a valid PMTiles archive.
         */
        PMTilesTileDataSource(int minZoom, int maxZoom, const std::string& path);
    
        virtual ~PMTilesTileDataSource();
        
        /**
         * Get PMTiles archive metadata information as a JSON string.
         * The metadata is stored in the PMTiles archive and may contain
         * information such as name, description, attribution, vector layers, etc.
         * @return JSON string containing metadata, or empty string if no metadata exists.
         */
        std::string getMetaData() const;
        
        /**
         * Get a specific metadata value by key from the parsed JSON metadata.
         * @param key The metadata key to retrieve (e.g., "name", "description", "attribution").
         * @return The metadata value as a string, or empty string if not found.
         */
        std::string getMetaData(const std::string& key) const;

        virtual int getMinZoom() const;

        virtual int getMaxZoom() const;

        virtual MapBounds getDataExtent() const;

        virtual std::shared_ptr<TileData> loadTile(const MapTile& mapTile);

    private:
        struct PMTilesHeader {
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

        struct DirectoryEntry {
            uint64_t tileId;
            uint64_t offset;
            uint32_t length;
            uint32_t runLength;
        };

        static PMTilesHeader ReadHeader(std::ifstream& file);
        static std::vector<uint8_t> DecompressData(const std::vector<uint8_t>& data, uint8_t compression);
        static std::vector<DirectoryEntry> DecodeDirectory(const std::vector<uint8_t>& data);
        static uint64_t ReadVarint(const std::vector<uint8_t>& data, size_t& offset);
        static uint64_t ZxyToTileId(int z, int x, int y);
        static void TileIdToZxy(uint64_t tileId, int& z, int& x, int& y);
        
        std::vector<uint8_t> ReadTileData(uint64_t offset, uint32_t length);
        const DirectoryEntry* FindTileEntry(uint64_t tileId);
        std::vector<DirectoryEntry> LoadLeafDirectory(uint64_t offset, uint32_t length);

        std::string _path;
        std::unique_ptr<std::ifstream> _file;
        PMTilesHeader _header;
        std::vector<DirectoryEntry> _rootDirectory;
        mutable std::optional<std::string> _cachedMetadata;
        mutable std::optional<MapBounds> _cachedDataExtent;
        mutable std::recursive_mutex _mutex;
    };
    
}

#endif

#endif
