//
//  NTValhallaRoutingService.mm
//  routing-lib iOS wrapper
//

#import "NTValhallaRoutingService.h"

#include "../native/routing/ValhallaRoutingService.h"
#include "../native/routing/ValhallaOnlineRoutingService.h"

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
// JSON serialisation helpers
// ---------------------------------------------------------------------------

/**
 * Build a Valhalla route-request JSON body from an NTRoutingRequest.
 * The "costing" key is only included when the request's profile is non-nil;
 * the service will inject its own profile if absent.
 */
static NSString *serializeRoutingRequest(NTRoutingRequest *req) {
    NSMutableString *sb = [NSMutableString string];
    [sb appendString:@"{\"locations\":["];
    NSArray<NTLatLon *> *points = req.points;
    for (NSUInteger i = 0; i < points.count; i++) {
        if (i > 0) [sb appendString:@","];
        NTLatLon *ll = points[i];
        [sb appendFormat:@"{\"lon\":%g,\"lat\":%g}", ll.lon, ll.lat];
    }
    [sb appendString:@"]"];
    if (req.profile.length > 0) {
        [sb appendFormat:@",\"costing\":\"%@\"", req.profile];
    }
    if (req.customJSON.length > 0) {
        // Merge top-level keys from customJSON into the body.
        // customJSON must be a JSON object string, e.g. {"units":"miles"}.
        NSString *inner = req.customJSON;
        NSRange openBrace  = [inner rangeOfString:@"{"];
        NSRange closeBrace = [inner rangeOfString:@"}" options:NSBackwardsSearch];
        if (openBrace.location != NSNotFound && closeBrace.location != NSNotFound) {
            NSString *content = [inner substringWithRange:
                NSMakeRange(openBrace.location + 1,
                            closeBrace.location - openBrace.location - 1)];
            if (content.length > 0) {
                [sb appendFormat:@",%@", content];
            }
        }
    }
    [sb appendString:@"}"];
    return sb;
}

/**
 * Build a Valhalla trace-attributes-request JSON body from an NTRouteMatchingRequest.
 */
static NSString *serializeRouteMatchingRequest(NTRouteMatchingRequest *req) {
    NSMutableString *sb = [NSMutableString string];
    [sb appendString:@"{\"shape\":["];
    NSArray<NTLatLon *> *points = req.points;
    for (NSUInteger i = 0; i < points.count; i++) {
        if (i > 0) [sb appendString:@","];
        NTLatLon *ll = points[i];
        [sb appendFormat:@"{\"lon\":%g,\"lat\":%g}", ll.lon, ll.lat];
    }
    [sb appendString:@"],\"shape_match\":\"map_snap\""];
    if (req.accuracy > 0.0f) {
        [sb appendFormat:@",\"gps_accuracy\":%g", (double)req.accuracy];
    }
    if (req.profile.length > 0) {
        [sb appendFormat:@",\"costing\":\"%@\"", req.profile];
    }
    if (req.customJSON.length > 0) {
        NSString *inner = req.customJSON;
        NSRange openBrace  = [inner rangeOfString:@"{"];
        NSRange closeBrace = [inner rangeOfString:@"}" options:NSBackwardsSearch];
        if (openBrace.location != NSNotFound && closeBrace.location != NSNotFound) {
            NSString *content = [inner substringWithRange:
                NSMakeRange(openBrace.location + 1,
                            closeBrace.location - openBrace.location - 1)];
            if (content.length > 0) {
                [sb appendFormat:@",%@", content];
            }
        }
    }
    [sb appendString:@"}"];
    return sb;
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
        [self addMBTilesPath:path];
    }
    return self;
}

- (void)addMBTilesPath:(NSString *)path {
    _service->addMBTilesPath(path.UTF8String);
}

- (NSString *)profile {
    return [NSString stringWithUTF8String:_service->getProfile().c_str()];
}

- (void)setProfile:(NSString *)profile {
    _service->setProfile(profile.UTF8String);
}

- (nullable NSString *)configurationParameterForKey:(NSString *)key {
    std::string val = _service->getConfigurationParameter(key.UTF8String);
    if (val.empty()) return nil;
    return [NSString stringWithUTF8String:val.c_str()];
}

- (void)setConfigurationParameter:(NSString *)jsonValue forKey:(NSString *)key {
    try {
        _service->setConfigurationParameter(key.UTF8String, jsonValue.UTF8String);
    } catch (...) {}
}

- (void)addLocaleWithKey:(NSString *)key json:(NSString *)json {
    _service->addLocale(key.UTF8String, json.UTF8String);
}

- (nullable NSString *)calculateRoute:(NTRoutingRequest *)request
                                error:(NSError * _Nullable __autoreleasing *)error {
    NSString *jsonBody = serializeRoutingRequest(request);
    return [self callRaw:@"route" jsonBody:jsonBody error:error];
}

- (nullable NSString *)matchRoute:(NTRouteMatchingRequest *)request
                            error:(NSError * _Nullable __autoreleasing *)error {
    NSString *jsonBody = serializeRouteMatchingRequest(request);
    return [self callRaw:@"trace_attributes" jsonBody:jsonBody error:error];
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

- (instancetype)initWithBaseURL:(NSString *)baseURL {
    return [self initWithBaseURL:baseURL handler:nil];
}

- (instancetype)initWithBaseURL:(NSString *)baseURL
                        handler:(NTHTTPPostHandler _Nullable)handler {
    self = [super init];
    if (!self) return nil;

    if (handler) {
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
    } else {
        _service = std::make_shared<routing::ValhallaOnlineRoutingService>(
            baseURL.UTF8String);
    }
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
    NSString *jsonBody = serializeRoutingRequest(request);
    return [self callRaw:@"route" jsonBody:jsonBody error:error];
}

- (nullable NSString *)matchRoute:(NTRouteMatchingRequest *)request
                            error:(NSError * _Nullable __autoreleasing *)error {
    NSString *jsonBody = serializeRouteMatchingRequest(request);
    return [self callRaw:@"trace_attributes" jsonBody:jsonBody error:error];
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
