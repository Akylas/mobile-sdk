// ---------------------------------------------------------------------------
// AFNetworking integration example for NTValhallaRoutingService (iOS, ObjC)
// ---------------------------------------------------------------------------
//
// Requires AFNetworking (pod 'AFNetworking', ~> 4.0).
//
// NTHTTPPostHandler is a synchronous block, so we use a semaphore to
// turn AFNetworking's async completion into a blocking call.
//
// Call all routing methods off the main thread — e.g. from a serial
// background queue — to avoid deadlocks and main-thread warnings.

#import "NTValhallaRoutingService.h"
#import <AFNetworking/AFNetworking.h>

// ---------------------------------------------------------------------------
// Factory — build NTValhallaOnlineRoutingService backed by AFNetworking
// ---------------------------------------------------------------------------

static NTValhallaOnlineRoutingService *
BuildOnlineServiceWithAFNetworking(NSString *baseURL, NSString *profile) {

    // Shared session manager — reuse across calls for connection pooling.
    static AFHTTPSessionManager *manager;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        manager = [AFHTTPSessionManager manager];
        manager.requestSerializer  = [AFJSONRequestSerializer serializer];
        manager.responseSerializer = [AFHTTPResponseSerializer serializer]; // raw data
        manager.requestSerializer.timeoutInterval = 30.0;
    });

    NTHTTPPostHandler handler = ^NSString *(NSString *url,
                                             NSString *postBody,
                                             NSError **outError) {
        __block NSString *result = nil;
        __block NSError  *blockError = nil;

        dispatch_semaphore_t sem = dispatch_semaphore_create(0);

        // Parse the body back to a dictionary so AFNetworking can serialize it.
        NSData *bodyData = [postBody dataUsingEncoding:NSUTF8StringEncoding];
        NSDictionary *params = [NSJSONSerialization JSONObjectWithData:bodyData
                                                               options:0
                                                                 error:nil];

        [manager POST:url parameters:params
        headers:@{@"Accept": @"application/json"}
            progress:nil
             success:^(NSURLSessionDataTask *task, id responseObject) {
            if ([responseObject isKindOfClass:[NSData class]]) {
                result = [[NSString alloc] initWithData:responseObject
                                              encoding:NSUTF8StringEncoding];
            } else if ([responseObject isKindOfClass:[NSString class]]) {
                result = responseObject;
            } else {
                NSData *jsonData = [NSJSONSerialization dataWithJSONObject:responseObject
                                                                   options:0
                                                                     error:nil];
                result = jsonData ? [[NSString alloc] initWithData:jsonData
                                                          encoding:NSUTF8StringEncoding]
                                  : @"{}";
            }
            dispatch_semaphore_signal(sem);
        } failure:^(NSURLSessionDataTask *task, NSError *error) {
            blockError = error;
            dispatch_semaphore_signal(sem);
        }];

        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

        if (outError) *outError = blockError;
        return result;
    };

    NTValhallaOnlineRoutingService *service =
        [[NTValhallaOnlineRoutingService alloc] initWithBaseURL:baseURL
                                                        handler:handler];
    service.profile = profile ?: @"pedestrian";
    return service;
}

// ---------------------------------------------------------------------------
// Usage examples
// ---------------------------------------------------------------------------

/// Online route — run from a background queue.
static void ExampleOnlineRoute(void) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{

        NTValhallaOnlineRoutingService *service =
            BuildOnlineServiceWithAFNetworking(
                @"https://valhalla1.openstreetmap.de",
                @"pedestrian");

        NTRoutingRequest *req = [[NTRoutingRequest alloc] initWithPoints:@[
            [NTLatLon lat:48.8566 lon:2.3522],   // Paris
            [NTLatLon lat:48.8738 lon:2.2950],   // Bois de Boulogne
        ]];
        req.customJSON = @"{\"units\":\"kilometers\"}";

        NSError *error = nil;
        NSString *rawJSON = [service calculateRoute:req error:&error];
        if (rawJSON) {
            NSLog(@"Route JSON: %@", rawJSON);
        } else {
            NSLog(@"Error: %@", error.localizedDescription);
        }
    });
}

/// Offline route using MBTiles — no network required.
static void ExampleOfflineRoute(NSString *mbtilesPath) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{

        NTValhallaRoutingService *service =
            [[NTValhallaRoutingService alloc] initWithMBTilesPaths:@[mbtilesPath]];
        service.profile = @"auto";

        NTRoutingRequest *req = [[NTRoutingRequest alloc] initWithPoints:@[
            [NTLatLon lat:48.8566 lon:2.3522],
            [NTLatLon lat:48.8738 lon:2.2950],
        ]];

        NSError *error = nil;
        NSString *rawJSON = [service calculateRoute:req error:&error];
        if (rawJSON) NSLog(@"Offline route: %@", rawJSON);
    });
}

/// Isochrone — direct endpoint call.
static void ExampleIsochrone(void) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{

        NTValhallaOnlineRoutingService *service =
            BuildOnlineServiceWithAFNetworking(
                @"https://valhalla1.openstreetmap.de",
                @"pedestrian");

        NSString *body = @"{"
            "\"locations\":[{\"lon\":2.3522,\"lat\":48.8566}],"
            "\"costing\":\"pedestrian\","
            "\"contours\":[{\"time\":10},{\"time\":20}]"
        "}";

        NSError *error = nil;
        NSString *rawJSON = [service callRaw:@"isochrone" jsonBody:body error:&error];
        if (rawJSON) NSLog(@"Isochrone JSON: %@", rawJSON);
    });
}

/// Map-matching a GPS trace.
static void ExampleMapMatch(void) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{

        NTValhallaOnlineRoutingService *service =
            BuildOnlineServiceWithAFNetworking(
                @"https://valhalla1.openstreetmap.de",
                @"pedestrian");

        NTRouteMatchingRequest *req = [[NTRouteMatchingRequest alloc] initWithPoints:@[
            [NTLatLon lat:48.8566 lon:2.3522],
            [NTLatLon lat:48.8600 lon:2.3600],
            [NTLatLon lat:48.8650 lon:2.3700],
        ] accuracy:15.0f];

        NSError *error = nil;
        NSString *rawJSON = [service matchRoute:req error:&error];
        if (rawJSON) NSLog(@"Map-match JSON: %@", rawJSON);
    });
}
