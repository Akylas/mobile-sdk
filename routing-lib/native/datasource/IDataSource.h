#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace routing {

    /**
     * Abstract tile data source interface for the routing engine.
     *
     * Implementations may back this with MBTiles SQLite files, PMTiles archives,
     * or any other tile storage format.
     *
     * Tiles are returned as raw bytes (possibly gzip- or zstd-compressed).
     * Decompression is the responsibility of the consumer (ValhallaRoutingService).
     */
    class IDataSource {
    public:
        virtual ~IDataSource() = default;

        /**
         * Retrieve raw tile bytes for the given tile coordinates.
         * @return Tile bytes, or std::nullopt if the tile is not present in this source.
         */
        virtual std::optional<std::vector<uint8_t>> getTile(int zoom, int x, int y) = 0;

        /**
         * Query a metadata value by key (e.g. "format", "minzoom", "maxzoom", "bounds").
         * @return The string value, or empty string if the key is not present.
         */
        virtual std::string getMetadata(const std::string& key) = 0;
    };

} // namespace routing
