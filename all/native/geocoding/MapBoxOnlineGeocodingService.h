/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_MAPBOXONLINEGEOCODINGSERVICE_H_
#define _CARTO_MAPBOXONLINEGEOCODINGSERVICE_H_

#if defined(_CARTO_GEOCODING_SUPPORT)

#include "geocoding/GeocodingService.h"

namespace carto {

    /**
     * An online geocoding service that uses MapBox geocoder.
     * As the class connects to an external (non-CARTO) service, this class is provided "as-is",
     * future changes from the service provider may not be compatible with the implementation.
     * Be sure to read the Terms and Conditions of the MapBox service to see if the
     * service is available for your application.
     * Note: this class is experimental and may change or even be removed in future SDK versions.
     */
    class MapBoxOnlineGeocodingService : public GeocodingService {
    public:
        /**
         * Constructs a new instance of the MapBoxOnlineGeocodingService given API key.
         * @param accessToken The access token to use (registered with MapBox).
         */
        explicit MapBoxOnlineGeocodingService(const std::string& accessToken);
        virtual ~MapBoxOnlineGeocodingService();

        /**
         * Returns the custom backend service URL.
         * @return The custom backend service URL. If this is not defined, an empty string is returned.
         */
        std::string getCustomServiceURL() const;
        /**
         * Sets the custom backend service URL. 
         * The custom URL should contain tags "{query}" and "{access_token}" that will be substituted by the SDK.
         * @param serviceURL The custom backend service URL to use. If this is empty, then the default service is used.
         */
        void setCustomServiceURL(const std::string& serviceURL);

        virtual bool isAutocomplete() const;
        virtual void setAutocomplete(bool autocomplete);

        virtual std::string getLanguage() const;
        virtual void setLanguage(const std::string& lang);

        virtual int getMaxResults() const;
        virtual void setMaxResults(int maxResults);

        virtual std::vector<std::shared_ptr<GeocodingResult> > calculateAddresses(const std::shared_ptr<GeocodingRequest>& request) const;

    protected:
        static const std::string MAPBOX_SERVICE_URL;

        const std::string _accessToken;
        bool _autocomplete;
        std::string _language;
        int _maxResults;
        std::string _serviceURL;

        mutable std::mutex _mutex;
    };
    
}

#endif

#endif
