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
     * Standalone Valhalla routing service.
     *
     * Consolidates the functionality of ValhallaOfflineRoutingService and
     * MultiValhallaOfflineRoutingService from the Carto SDK into a single
     * self-contained class that has no dependency on the Carto core libraries.
     *
     * Data sources (MBTiles SQLite files, or any IDataSource implementation)
     * are registered via the constructor or addSource(). Multiple sources are
     * queried in order so that the world can be split across several MBTiles
     * files with non-overlapping geographic coverage.
     *
     * Locale strings for navigation instructions can be registered with
     * addLocale().
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
        // Valhalla configuration (dot-delimited key paths, JSON values)
        //
        // Example:
        //   service.setConfigurationParameter("costing_options.auto.use_highways",
        //                                     Variant(0.0));
        // ----------------------------------------------------------------

        Variant getConfigurationParameter(const std::string& param) const;
        void setConfigurationParameter(const std::string& param, const Variant& value);

        // ----------------------------------------------------------------
        // Locale support for narrative generation
        // ----------------------------------------------------------------

        /**
         * Register a locale JSON blob so that valhalla can produce
         * navigation instructions in the given language.
         * @param key  Locale identifier (e.g. "en-US", "de-DE")
         * @param json Locale JSON string (valhalla locale format)
         */
        void addLocale(const std::string& key, const std::string& json);

        // ----------------------------------------------------------------
        // Routing API
        // ----------------------------------------------------------------

        std::shared_ptr<RoutingResult>       calculateRoute(const std::shared_ptr<RoutingRequest>& request) const;
        std::shared_ptr<RouteMatchingResult> matchRoute(const std::shared_ptr<RouteMatchingRequest>& request) const;

    private:
        // C++17 helper: split dot-delimited key path into segments
        static std::vector<std::string> splitKeys(const std::string& param);

        mutable std::mutex _mutex;
        std::vector<std::shared_ptr<IDataSource>> _sources;
        std::string _profile;
        Variant     _configuration;
    };

} // namespace routing
