#pragma once

#include "../core/Variant.h"
#include "../datasource/IDataSource.h"
#include "../routing/RoutingRequest.h"
#include "../routing/RoutingResult.h"
#include "../routing/RouteMatchingRequest.h"
#include "../routing/RouteMatchingResult.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace routing {

    /**
     * Offline Valhalla routing service.
     *
     * Data sources (MBTiles SQLite files, or any IDataSource implementation)
     * are registered via the constructor or addSource(). Multiple sources are
     * queried so that the world can be split across several MBTiles files.
     *
     * Routing results are returned as raw Valhalla JSON strings. Parsing of
     * individual fields is the responsibility of the application layer.
     *
     * Locale strings for narrative generation can be registered via addLocale().
     */
    class ValhallaRoutingService {
    public:
        /**
         * Construct with an initial set of data sources.
         */
        explicit ValhallaRoutingService(
            std::vector<std::shared_ptr<IDataSource>> sources = {});
        virtual ~ValhallaRoutingService();

        // ----------------------------------------------------------------
        // Source management
        // ----------------------------------------------------------------

        void addSource(std::shared_ptr<IDataSource> source);
        bool removeSource(const std::shared_ptr<IDataSource>& source);
        std::vector<std::shared_ptr<IDataSource>> getSources() const;

        // ----------------------------------------------------------------
        // Profile
        // ----------------------------------------------------------------

        std::string getProfile() const;
        void setProfile(const std::string& profile);

        // ----------------------------------------------------------------
        // Valhalla configuration (dot-delimited key paths, Variant values)
        //
        // Example:
        //   svc.setConfigurationParameter("costing_options.auto.use_highways",
        //                                 Variant(0.0));
        // ----------------------------------------------------------------

        Variant getConfigurationParameter(const std::string& param) const;
        void setConfigurationParameter(const std::string& param, const Variant& value);

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
        // Routing API — returns raw Valhalla JSON strings
        // ----------------------------------------------------------------

        /**
         * Calculate a route. Returns the raw Valhalla JSON response.
         */
        std::shared_ptr<RoutingResult> calculateRoute(
            const std::shared_ptr<RoutingRequest>& request) const;

        /**
         * Match a GPS trace to the road network. Returns the raw JSON response.
         */
        std::shared_ptr<RouteMatchingResult> matchRoute(
            const std::shared_ptr<RouteMatchingRequest>& request) const;

        /**
         * Call any Valhalla API endpoint directly with a pre-built JSON request.
         * @param endpoint  Endpoint name: "route", "trace_attributes", "trace_route",
         *                  "matrix", "isochrone", "locate", "height",
         *                  "expansion", "centroid", "status".
         * @param jsonBody  Full Valhalla request JSON string.
         * @return          Raw Valhalla JSON response string.
         */
        std::string callRaw(const std::string& endpoint,
                            const std::string& jsonBody) const;

    private:
        static std::vector<std::string> splitKeys(const std::string& param);
        static Variant setNestedValue(Variant obj,
                                      const std::vector<std::string>& keys,
                                      int idx,
                                      const Variant& value);

        mutable std::mutex _mutex;
        std::vector<std::shared_ptr<IDataSource>> _sources;
        std::string _profile;
        Variant     _configuration;
    };

} // namespace routing
