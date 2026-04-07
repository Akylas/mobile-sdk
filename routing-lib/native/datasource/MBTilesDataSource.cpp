#include "MBTilesDataSource.h"

#include <sqlite3.h>
#include <stdexcept>
#include <cstring>

namespace routing {

    MBTilesDataSource::MBTilesDataSource(const std::string& path) : _path(path) {
        int rc = sqlite3_open_v2(path.c_str(), &_db,
                                 SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr);
        if (rc != SQLITE_OK) {
            std::string err = _db ? sqlite3_errmsg(_db) : "unknown error";
            if (_db) sqlite3_close(_db);
            _db = nullptr;
            throw std::runtime_error("MBTilesDataSource: cannot open '" + path + "': " + err);
        }
        // Performance tuning for read-only access
        sqlite3_exec(_db, "PRAGMA journal_mode=OFF;", nullptr, nullptr, nullptr);
        sqlite3_exec(_db, "PRAGMA synchronous=OFF;",  nullptr, nullptr, nullptr);
        sqlite3_exec(_db, "PRAGMA cache_size=2000;",  nullptr, nullptr, nullptr);
    }

    MBTilesDataSource::~MBTilesDataSource() {
        if (_db) {
            sqlite3_close(_db);
            _db = nullptr;
        }
    }

    std::optional<std::vector<uint8_t>> MBTilesDataSource::getTile(int zoom, int x, int y) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_db) return std::nullopt;

        // MBTiles uses TMS y (flipped): tile_row = (1 << zoom) - 1 - y
        int tmsY = (1 << zoom) - 1 - y;

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT tile_data FROM tiles "
                          "WHERE zoom_level=? AND tile_column=? AND tile_row=?";
        if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return std::nullopt;
        }
        sqlite3_bind_int(stmt, 1, zoom);
        sqlite3_bind_int(stmt, 2, x);
        sqlite3_bind_int(stmt, 3, tmsY);

        std::optional<std::vector<uint8_t>> result;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int bytes = sqlite3_column_bytes(stmt, 0);
            if (blob && bytes > 0) {
                const uint8_t* data = static_cast<const uint8_t*>(blob);
                result = std::vector<uint8_t>(data, data + bytes);
            }
        }
        sqlite3_finalize(stmt);
        return result;
    }

    std::string MBTilesDataSource::getMetadata(const std::string& key) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_db) return {};

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT value FROM metadata WHERE name=?";
        if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return {};
        }
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

        std::string value;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (text) value = text;
        }
        sqlite3_finalize(stmt);
        return value;
    }

    sqlite3* MBTilesDataSource::getDatabase() const { return _db; }

    const std::string& MBTilesDataSource::getPath() const { return _path; }

} // namespace routing
