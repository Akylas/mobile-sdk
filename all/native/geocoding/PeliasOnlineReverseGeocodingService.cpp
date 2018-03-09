#if defined(_CARTO_GEOCODING_SUPPORT)

#include "PeliasOnlineReverseGeocodingService.h"
#include "core/BinaryData.h"
#include "components/Exceptions.h"
#include "geocoding/PeliasGeocodingProxy.h"
#include "projections/Projection.h"
#include "utils/GeneralUtils.h"
#include "utils/NetworkUtils.h"
#include "utils/Log.h"

#include <boost/lexical_cast.hpp>

namespace carto {

    PeliasOnlineReverseGeocodingService::PeliasOnlineReverseGeocodingService(const std::string& apiKey) :
        _apiKey(apiKey),
        _serviceURL(),
        _mutex()
    {
    }

    PeliasOnlineReverseGeocodingService::~PeliasOnlineReverseGeocodingService() {
    }

    std::string PeliasOnlineReverseGeocodingService::getCustomServiceURL() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _serviceURL;
    }

    void PeliasOnlineReverseGeocodingService::setCustomServiceURL(const std::string& serviceURL) {
        std::lock_guard<std::mutex> lock(_mutex);
        _serviceURL = serviceURL;
    }

    std::vector<std::shared_ptr<GeocodingResult> > PeliasOnlineReverseGeocodingService::calculateAddresses(const std::shared_ptr<ReverseGeocodingRequest>& request) const {
        if (!request) {
            throw NullArgumentException("Null request");
        }

        MapPos point = request->getProjection()->toWgs84(request->getLocation());

        std::string baseURL;

        std::map<std::string, std::string> params;
        {
            std::map<std::string, std::string> tagMap;
            tagMap["api_key"] = NetworkUtils::URLEncode(_apiKey);
            baseURL = GeneralUtils::ReplaceTags(_serviceURL.empty() ? MAPZEN_SERVICE_URL : _serviceURL, tagMap);

            params["point.lat"] = boost::lexical_cast<std::string>(point.getY());
            params["point.lon"] = boost::lexical_cast<std::string>(point.getX());
            params["boundary.circle.lat"] = boost::lexical_cast<std::string>(point.getY());
            params["boundary.circle.lon"] = boost::lexical_cast<std::string>(point.getX());
            params["boundary.circle.radius"] = boost::lexical_cast<std::string>(request->getSearchRadius());
        }

        std::string url = NetworkUtils::BuildURLFromParameters(baseURL, params);
        Log::Debugf("PeliasOnlineReverseGeocodingService::calculateAddresses: Loading %s", url.c_str());

        std::shared_ptr<BinaryData> responseData;
        if (!NetworkUtils::GetHTTP(url, responseData, Log::IsShowDebug())) {
            throw NetworkException("Failed to fetch response");
        }

        std::string responseString;
        if (responseData) {
            responseString = std::string(reinterpret_cast<const char*>(responseData->data()), responseData->size());
        } else {
            throw GenericException("Empty response");
        }

        return PeliasGeocodingProxy::ReadResponse(responseString, request->getProjection());
    }

    const std::string PeliasOnlineReverseGeocodingService::MAPZEN_SERVICE_URL = "https://search.mapzen.com/v1/reverse?api_key={api_key}";
}

#endif
