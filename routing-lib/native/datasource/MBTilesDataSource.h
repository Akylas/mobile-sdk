#pragma once

#include "IDataSource.h"

#include <memory>
#include <mutex>
#include <string>

struct sqlite3;

namespace routing {

    /**
     * A tile data source backed by a single MBTiles SQLite file.
     *
     * The MBTiles spec stores tiles in a table called "tiles" with columns
     * (zoom_level, tile_column, tile_row, tile_data).  The tile_row in MBTiles
     * is flipped compared to XYZ convention (row = (1<<zoom) - 1 - y).
     *
     * Tile bytes are returned as-is (may be gzip-compressed per the spec).
     *
     * The underlying sqlite3 handle is also exposed so that it can be passed
     * directly to valhalla::baldr::GraphReader when the MBTiles contains
     * valhalla graph tiles instead of raster/vector map tiles.
     */
    class MBTilesDataSource : public IDataSource {
    public:
        /**
         * Open an MBTiles file at the given path.
         * @throws std::runtime_error if the file cannot be opened.
         */
        explicit MBTilesDataSource(const std::string& path);
        virtual ~MBTilesDataSource();

        // IDataSource interface
        std::optional<std::vector<uint8_t>> getTile(int zoom, int x, int y) override;
        std::string getMetadata(const std::string& key) override;

        /**
         * Returns the raw sqlite3 handle for direct use by valhalla's GraphReader.
         * The returned pointer is owned by this object; do not close it externally.
         */
        sqlite3* getDatabase();

        /**
         * Returns the file path this source was opened from.
         */
        const std::string& getPath() const;

    private:
        std::string _path;
        sqlite3* _db = nullptr;
        mutable std::mutex _mutex;

        void openDatabase();
    };

} // namespace routing
