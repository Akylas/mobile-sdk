//
//  NTValhallaRoutingService.h
//  routing-lib iOS wrapper
//
//  Objective-C bridge to the C++ ValhallaRoutingService (offline) and
//  ValhallaOnlineRoutingService (online with user-supplied HTTP handler).
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// ---------------------------------------------------------------------------
// NTRoutingRequest — input for calculateRoute
// ---------------------------------------------------------------------------

/**
 * A geographic coordinate (WGS-84).
 */
@interface NTLatLon : NSObject
@property (nonatomic, assign) double lat;
@property (nonatomic, assign) double lon;
+ (instancetype)lat:(double)lat lon:(double)lon;
@end


/**
 * Input to a route calculation.
 * Set customJSON to override any Valhalla request fields (merged at the top level).
 */
@interface NTRoutingRequest : NSObject
/** Ordered list of NTLatLon waypoints. */
@property (nonatomic, copy) NSArray<NTLatLon *> *points;
/** Optional extra Valhalla request fields serialized as a JSON string. */
@property (nonatomic, copy, nullable) NSString *customJSON;

- (instancetype)initWithPoints:(NSArray<NTLatLon *> *)points;
@end


/**
 * Input to a route-matching (map-snapping) operation.
 */
@interface NTRouteMatchingRequest : NSObject
/** Ordered GPS trace points. */
@property (nonatomic, copy) NSArray<NTLatLon *> *points;
/** GPS accuracy in metres (0 = use Valhalla default). */
@property (nonatomic, assign) float accuracy;
/** Optional extra Valhalla request fields as a JSON string. */
@property (nonatomic, copy, nullable) NSString *customJSON;

- (instancetype)initWithPoints:(NSArray<NTLatLon *> *)points accuracy:(float)accuracy;
@end


// ---------------------------------------------------------------------------
// NTValhallaRoutingService — offline service backed by MBTiles
// ---------------------------------------------------------------------------

/**
 * Offline Valhalla routing service.
 *
 * Add one or more MBTiles database paths via -addMBTilesPath:.
 * All routing results are returned as raw Valhalla JSON strings; parsing
 * is the responsibility of the caller.
 *
 * All routing methods are synchronous and must be called from a background thread.
 */
@interface NTValhallaRoutingService : NSObject

/**
 * Initialise with an optional array of MBTiles file paths.
 */
- (instancetype)initWithMBTilesPaths:(nullable NSArray<NSString *> *)paths;

/** Add an MBTiles data source by file path. */
- (void)addMBTilesPath:(NSString *)path error:(NSError * _Nullable __autoreleasing *)error;

// -- Profile ----------------------------------------------------------------

/** Valhalla costing model, e.g. @"pedestrian", @"auto", @"bicycle". Default: @"pedestrian". */
@property (nonatomic, copy) NSString *profile;

// -- Configuration ----------------------------------------------------------

/**
 * Get a Valhalla configuration value by dot-delimited key path.
 * Returns nil if the key does not exist.
 * The returned value is a JSON string of that node.
 */
- (nullable NSString *)configurationParameterForKey:(NSString *)key;

/**
 * Set a Valhalla configuration value by dot-delimited key path.
 * @param jsonValue  JSON-encoded value (e.g. @"0.5", @"\"residential\"", @"true").
 */
- (void)setConfigurationParameter:(NSString *)jsonValue forKey:(NSString *)key;

// -- Locales ----------------------------------------------------------------

/**
 * Register a Valhalla locale so that narrative instructions are generated
 * in the given language.
 */
- (void)addLocaleWithKey:(NSString *)key json:(NSString *)json;

// -- Routing API ------------------------------------------------------------

/**
 * Calculate a route. Returns raw Valhalla JSON, or nil on error.
 * @param error  Populated on failure.
 */
- (nullable NSString *)calculateRoute:(NTRoutingRequest *)request
                                error:(NSError * _Nullable __autoreleasing *)error;

/**
 * Match a GPS trace to the road network. Returns raw Valhalla JSON, or nil on error.
 */
- (nullable NSString *)matchRoute:(NTRouteMatchingRequest *)request
                            error:(NSError * _Nullable __autoreleasing *)error;

/**
 * Call any Valhalla API endpoint directly.
 * @param endpoint   e.g. @"route", @"trace_attributes", @"isochrone", @"matrix" …
 * @param jsonBody   Pre-built Valhalla request JSON.
 * @return           Raw Valhalla JSON response, or nil on error.
 */
- (nullable NSString *)callRaw:(NSString *)endpoint
                      jsonBody:(NSString *)jsonBody
                         error:(NSError * _Nullable __autoreleasing *)error;

@end


// ---------------------------------------------------------------------------
// NTValhallaOnlineRoutingService — online service via HTTP
// ---------------------------------------------------------------------------

/**
 * Synchronous HTTP POST handler block.
 *
 * @param url       Full request URL.
 * @param postBody  JSON request body.
 * @param error     Set this on failure; returning nil triggers the error path.
 * @return          HTTP response body string, or nil on failure.
 */
typedef NSString * _Nullable (^NTHTTPPostHandler)(NSString *url,
                                                   NSString *postBody,
                                                   NSError * _Nullable __autoreleasing *error);

/**
 * Online Valhalla routing service.
 *
 * HTTP transport is handled by the user-supplied NTHTTPPostHandler block,
 * keeping this class independent of any specific HTTP stack (URLSession,
 * Alamofire, etc.).
 *
 * All routing methods are synchronous; call them from a background thread.
 */
@interface NTValhallaOnlineRoutingService : NSObject

/**
 * @param baseURL  Base URL, e.g. @"https://valhalla1.openstreetmap.de".
 * @param handler  Synchronous HTTP POST handler block.
 */
- (instancetype)initWithBaseURL:(NSString *)baseURL
                        handler:(NTHTTPPostHandler)handler;

/** Base URL of the Valhalla service. */
@property (nonatomic, copy) NSString *baseURL;

/** Valhalla costing model. Default: @"pedestrian". */
@property (nonatomic, copy) NSString *profile;

- (nullable NSString *)calculateRoute:(NTRoutingRequest *)request
                                error:(NSError * _Nullable __autoreleasing *)error;

- (nullable NSString *)matchRoute:(NTRouteMatchingRequest *)request
                            error:(NSError * _Nullable __autoreleasing *)error;

- (nullable NSString *)callRaw:(NSString *)endpoint
                      jsonBody:(NSString *)jsonBody
                         error:(NSError * _Nullable __autoreleasing *)error;

@end

NS_ASSUME_NONNULL_END
