#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sqlite3pp {
    class database;
}

namespace routing {

    /**
     * Offline Valhalla routing service backed by MBTiles SQLite files.
     *
     * Register one or more MBTiles file paths via addMBTilesPath(). Multiple
     * files are queried so that the world can be split across several databases.
     *
     * Databases are opened lazily on the first callRaw() invocation and closed
     * automatically once all in-flight requests have completed. Concurrent
     * requests from multiple threads are supported.
     *
     * Routing results are returned as raw Valhalla JSON strings. Parsing of
     * individual fields is the responsibility of the application layer.
     *
     * Locale strings for narrative generation can be registered via addLocale().
     */
    class ValhallaRoutingService {
    public:
        /**
         * Construct with an optional initial list of MBTiles file paths.
         */
        explicit ValhallaRoutingService(std::vector<std::string> paths = {});
        virtual ~ValhallaRoutingService();

        // ----------------------------------------------------------------
        // Database path management
        // ----------------------------------------------------------------

        /**
         * Add an MBTiles database by file path.
         * If databases are currently open (a request is in progress), the new
         * path will be used starting from the next request.
         */
        void addMBTilesPath(const std::string& path);

        std::vector<std::string> getMBTilesPaths() const;

        // ----------------------------------------------------------------
        // Profile (costing model)
        //
        // The profile is injected as "costing" into every callRaw() body
        // that does not already contain a "costing" key.
        // ----------------------------------------------------------------

        std::string getProfile() const;
        void setProfile(const std::string& profile);

        // ----------------------------------------------------------------
        // Valhalla configuration (dot-delimited key paths, JSON string values)
        //
        // Example:
        //   svc.setConfigurationParameter("costing_options.auto.use_highways",
        //                                 "0.0");
        // ----------------------------------------------------------------

        /**
         * Get a configuration value by dot-delimited key path.
         * Returns the JSON-serialized value, or an empty string if not found.
         */
        std::string getConfigurationParameter(const std::string& param) const;

        /**
         * Set a configuration value by dot-delimited key path.
         * @param param     Dot-delimited key path.
         * @param valueJson JSON-encoded value (e.g. "0.5", "\"residential\"", "true").
         */
        void setConfigurationParameter(const std::string& param, const std::string& valueJson);

        // ----------------------------------------------------------------
        // Locale support for narrative generation
        // ----------------------------------------------------------------

        /**
         * Register a locale JSON blob so that Valhalla can produce navigation
         * instructions in the given language.
         * @param key  Locale identifier (e.g. "en-US", "de-DE").
         * @param json Locale JSON string (Valhalla locale format).
         */
        void addLocale(const std::string& key, const std::string& json);

        // ----------------------------------------------------------------
        // Routing API
        // ----------------------------------------------------------------

        /**
         * Call any Valhalla API endpoint directly with a pre-built JSON request.
         *
         * The service's profile is injected as "costing" if the body does not
         * already contain that key.
         *
         * @param endpoint  Endpoint name: "route", "trace_attributes",
         *                  "trace_route", "matrix", "isochrone", "locate",
         *                  "height", "expansion", "centroid", "status".
         * @param jsonBody  Full Valhalla request JSON string.
         * @return          Raw Valhalla JSON response string.
         */
        std::string callRaw(const std::string& endpoint,
                            const std::string& jsonBody) const;

        std::string parseShape(const std::string& shape);

    private:
        // Database lifecycle helpers
        std::vector<std::shared_ptr<sqlite3pp::database>> acquireDatabases() const;
        void releaseDatabases() const;

        static std::vector<std::string> splitKeys(const std::string& param);

        mutable std::mutex          _mutex;
        std::vector<std::string>    _paths;          // registered MBTiles file paths
        mutable std::vector<std::shared_ptr<sqlite3pp::database>> _openDbs; // currently open handles
        mutable int                 _activeCount = 0; // in-flight request count
        std::string                 _profile;
        std::string                 _configuration;  // Valhalla config as JSON string
    };

} // namespace routing
