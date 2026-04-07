#pragma once

#include "IDataSource.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace routing {

    /**
     * Aggregates multiple IDataSource instances into a single logical source.
     *
     * getTile() tries each source in order and returns the first non-empty result.
     * This naturally handles the common case where the world is split into
     * multiple MBTiles files with non-overlapping geographic coverage.
     *
     * getMetadata() merges metadata: the first source that has a non-empty value
     * for the requested key wins.
     *
     * Sources can be added and removed at runtime (thread-safe).
     *
     * Tile decompression (gzip / zstd) is applied to the raw bytes returned by
     * each source before handing the data to the caller.
     */
    class MultiDataSource : public IDataSource {
    public:
        MultiDataSource() = default;
        explicit MultiDataSource(std::vector<std::shared_ptr<IDataSource>> sources);

        void addSource(std::shared_ptr<IDataSource> source);

        /**
         * Remove a source. Returns true if found and removed.
         */
        bool removeSource(const std::shared_ptr<IDataSource>& source);

        /**
         * Return snapshot of current sources (thread-safe copy).
         */
        std::vector<std::shared_ptr<IDataSource>> getSources() const;

        // IDataSource interface
        std::optional<std::vector<uint8_t>> getTile(int zoom, int x, int y) override;
        std::string getMetadata(const std::string& key) override;

    private:
        /**
         * Attempt to decompress raw bytes.
         * Tries gzip first, then zstd (if compiled with HAVE_ZSTD).
         * Returns the original bytes unchanged if decompression fails or is not needed.
         */
        static std::vector<uint8_t> decompress(const std::vector<uint8_t>& data);

        mutable std::mutex _mutex;
        std::vector<std::shared_ptr<IDataSource>> _sources;
    };

} // namespace routing
