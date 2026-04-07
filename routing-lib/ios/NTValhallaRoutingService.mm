//
//  NTValhallaRoutingService.mm
//  routing-lib iOS wrapper
//

#import "NTValhallaRoutingService.h"

#include "../native/routing/ValhallaRoutingService.h"
#include "../native/routing/ValhallaOnlineRoutingService.h"
#include "../native/datasource/MBTilesDataSource.h"
#include "../native/core/Projection.h"
#include "../native/core/EPSG4326.h"

#include <memory>
#include <stdexcept>

// ---------------------------------------------------------------------------
// NSError helper
// ---------------------------------------------------------------------------

static NSString * const NTRoutingErrorDomain = @"NTRoutingError";

static NSError *makeError(const std::exception& ex) {
    NSString *msg = [NSString stringWithUTF8String:ex.what()];
    return [NSError errorWithDomain:NTRoutingErrorDomain
                               code:-1
                           userInfo:@{NSLocalizedDescriptionKey: msg}];
}

// ---------------------------------------------------------------------------
// WGS-84 projection (shared)
// ---------------------------------------------------------------------------

static const std::shared_ptr<routing::Projection>& wgs84Projection() {
    static auto proj = std::make_shared<routing::EPSG4326>();
    return proj;
}

// ---------------------------------------------------------------------------
// NTLatLon
// ---------------------------------------------------------------------------

@implementation NTLatLon
+ (instancetype)lat:(double)lat lon:(double)lon {
    NTLatLon *c = [[NTLatLon alloc] init];
    c.lat = lat;
    c.lon = lon;
    return c;
}
@end

// ---------------------------------------------------------------------------
// NTRoutingRequest
// ---------------------------------------------------------------------------

@implementation NTRoutingRequest
- (instancetype)initWithPoints:(NSArray<NTLatLon *> *)points {
    self = [super init];
    if (self) { _points = [points copy]; }
    return self;
}
@end

// ---------------------------------------------------------------------------
// NTRouteMatchingRequest
// ---------------------------------------------------------------------------

@implementation NTRouteMatchingRequest
- (instancetype)initWithPoints:(NSArray<NTLatLon *> *)points accuracy:(float)accuracy {
    self = [super init];
    if (self) { _points = [points copy]; _accuracy = accuracy; }
    return self;
}
@end

// ---------------------------------------------------------------------------
// Internal helper: build routing::RoutingRequest from NTRoutingRequest
// ---------------------------------------------------------------------------

static std::shared_ptr<routing::RoutingRequest>
buildRoutingRequest(NTRoutingRequest *req) {
    std::vector<routing::MapPos> points;
    points.reserve([req.points count]);
    for (NTLatLon *ll in req.points) {
        // EPSG4326: x = lon, y = lat
        points.push_back(wgs84Projection()->fromLatLong(ll.lat, ll.lon));
    }
    auto r = std::make_shared<routing::RoutingRequest>(wgs84Projection(), points);
    if (req.customJSON.length > 0) {
        routing::Variant custom = routing::Variant::FromJSON(req.customJSON.UTF8String);
        if (custom.getType() == routing::VariantType::VARIANT_TYPE_OBJECT) {
            for (const auto& key : custom.getObjectKeys()) {
                r->setCustomParameter(key, custom.getObjectElement(key));
            }
        }
    }
    return r;
}

static std::shared_ptr<routing::RouteMatchingRequest>
buildRouteMatchingRequest(NTRouteMatchingRequest *req) {
    std::vector<routing::MapPos> points;
    points.reserve([req.points count]);
    for (NTLatLon *ll in req.points) {
        points.push_back(wgs84Projection()->fromLatLong(ll.lat, ll.lon));
    }
    auto r = std::make_shared<routing::RouteMatchingRequest>(
        wgs84Projection(), points, req.accuracy);
    if (req.customJSON.length > 0) {
        routing::Variant custom = routing::Variant::FromJSON(req.customJSON.UTF8String);
        if (custom.getType() == routing::VariantType::VARIANT_TYPE_OBJECT) {
            for (const auto& key : custom.getObjectKeys()) {
                r->setCustomParameter(key, custom.getObjectElement(key));
            }
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
// NTValhallaRoutingService
// ---------------------------------------------------------------------------

@interface NTValhallaRoutingService () {
    std::shared_ptr<routing::ValhallaRoutingService> _service;
}
@end

@implementation NTValhallaRoutingService

- (instancetype)initWithMBTilesPaths:(nullable NSArray<NSString *> *)paths {
    self = [super init];
    if (!self) return nil;

    _service = std::make_shared<routing::ValhallaRoutingService>();

    for (NSString *path in paths) {
        NSError *err = nil;
        [self addMBTilesPath:path error:&err];
        // Silently skip failures at construction time
    }
    return self;
}

- (void)addMBTilesPath:(NSString *)path
                 error:(NSError * _Nullable __autoreleasing *)error {
    try {
        auto src = std::make_shared<routing::MBTilesDataSource>(path.UTF8String);
        _service->addSource(src);
    } catch (const std::exception& ex) {
        if (error) *error = makeError(ex);
    }
}

- (NSString *)profile {
    return [NSString stringWithUTF8String:_service->getProfile().c_str()];
}

- (void)setProfile:(NSString *)profile {
    _service->setProfile(profile.UTF8String);
}

- (nullable NSString *)configurationParameterForKey:(NSString *)key {
    routing::Variant val = _service->getConfigurationParameter(key.UTF8String);
    if (val.getType() == routing::VariantType::VARIANT_TYPE_NULL) return nil;
    return [NSString stringWithUTF8String:val.toJSON().c_str()];
}

- (void)setConfigurationParameter:(NSString *)jsonValue forKey:(NSString *)key {
    try {
        routing::Variant val = routing::Variant::FromJSON(jsonValue.UTF8String);
        _service->setConfigurationParameter(key.UTF8String, val);
    } catch (...) {}
}

- (void)addLocaleWithKey:(NSString *)key json:(NSString *)json {
    _service->addLocale(key.UTF8String, json.UTF8String);
}

- (nullable NSString *)calculateRoute:(NTRoutingRequest *)request
                                error:(NSError * _Nullable __autoreleasing *)error {
    try {
        auto req = buildRoutingRequest(request);
        auto result = _service->calculateRoute(req);
        return [NSString stringWithUTF8String:result->getRawResult().c_str()];
    } catch (const std::exception& ex) {
        if (error) *error = makeError(ex);
        return nil;
    }
}

- (nullable NSString *)matchRoute:(NTRouteMatchingRequest *)request
                            error:(NSError * _Nullable __autoreleasing *)error {
    try {
        auto req = buildRouteMatchingRequest(request);
        auto result = _service->matchRoute(req);
        return [NSString stringWithUTF8String:result->getRawResult().c_str()];
    } catch (const std::exception& ex) {
        if (error) *error = makeError(ex);
        return nil;
    }
}

- (nullable NSString *)callRaw:(NSString *)endpoint
                      jsonBody:(NSString *)jsonBody
                         error:(NSError * _Nullable __autoreleasing *)error {
    try {
        std::string result = _service->callRaw(endpoint.UTF8String, jsonBody.UTF8String);
        return [NSString stringWithUTF8String:result.c_str()];
    } catch (const std::exception& ex) {
        if (error) *error = makeError(ex);
        return nil;
    }
}

@end

// ---------------------------------------------------------------------------
// NTValhallaOnlineRoutingService
// ---------------------------------------------------------------------------

@interface NTValhallaOnlineRoutingService () {
    std::shared_ptr<routing::ValhallaOnlineRoutingService> _service;
}
@end

@implementation NTValhallaOnlineRoutingService

- (instancetype)initWithBaseURL:(NSString *)baseURL
                        handler:(NTHTTPPostHandler)handler {
    self = [super init];
    if (!self) return nil;

    routing::ValhallaOnlineRoutingService::HttpHandler cppHandler =
        [handler](const std::string& url, const std::string& body) -> std::string {
            NSError *err = nil;
            NSString *nsURL  = [NSString stringWithUTF8String:url.c_str()];
            NSString *nsBody = [NSString stringWithUTF8String:body.c_str()];
            NSString *result = handler(nsURL, nsBody, &err);
            if (!result) {
                std::string what = err ? err.localizedDescription.UTF8String : "HTTP error";
                throw std::runtime_error(what);
            }
            return result.UTF8String;
        };

    _service = std::make_shared<routing::ValhallaOnlineRoutingService>(
        baseURL.UTF8String, std::move(cppHandler));
    return self;
}

- (NSString *)baseURL {
    return [NSString stringWithUTF8String:_service->getBaseURL().c_str()];
}

- (void)setBaseURL:(NSString *)baseURL {
    _service->setBaseURL(baseURL.UTF8String);
}

- (NSString *)profile {
    return [NSString stringWithUTF8String:_service->getProfile().c_str()];
}

- (void)setProfile:(NSString *)profile {
    _service->setProfile(profile.UTF8String);
}

- (nullable NSString *)calculateRoute:(NTRoutingRequest *)request
                                error:(NSError * _Nullable __autoreleasing *)error {
    try {
        auto req = buildRoutingRequest(request);
        auto result = _service->calculateRoute(req);
        return [NSString stringWithUTF8String:result->getRawResult().c_str()];
    } catch (const std::exception& ex) {
        if (error) *error = makeError(ex);
        return nil;
    }
}

- (nullable NSString *)matchRoute:(NTRouteMatchingRequest *)request
                            error:(NSError * _Nullable __autoreleasing *)error {
    try {
        auto req = buildRouteMatchingRequest(request);
        auto result = _service->matchRoute(req);
        return [NSString stringWithUTF8String:result->getRawResult().c_str()];
    } catch (const std::exception& ex) {
        if (error) *error = makeError(ex);
        return nil;
    }
}

- (nullable NSString *)callRaw:(NSString *)endpoint
                      jsonBody:(NSString *)jsonBody
                         error:(NSError * _Nullable __autoreleasing *)error {
    try {
        std::string result = _service->callRaw(endpoint.UTF8String, jsonBody.UTF8String);
        return [NSString stringWithUTF8String:result.c_str()];
    } catch (const std::exception& ex) {
        if (error) *error = makeError(ex);
        return nil;
    }
}

@end
