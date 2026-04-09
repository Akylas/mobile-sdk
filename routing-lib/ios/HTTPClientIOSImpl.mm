//
// HTTPClientIOSImpl.mm
// iOS/macOS implementation of routing::HTTPClient using NSURLSession.
//

#include "../../native/network/HTTPClient.h"

#import <Foundation/Foundation.h>

#include <stdexcept>
#include <string>

namespace routing {

HTTPClient::HTTPClient() {}

std::string HTTPClient::post(const std::string& url, const std::string& jsonBody) const {
    NSURL* nsURL = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
    if (!nsURL) {
        throw std::runtime_error("HTTPClient: Invalid URL: " + url);
    }

    NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:nsURL];
    [request setHTTPMethod:@"POST"];
    [request setValue:@"application/json; charset=utf-8" forHTTPHeaderField:@"Content-Type"];
    [request setValue:@"application/json"                forHTTPHeaderField:@"Accept"];

    NSData* bodyData = [NSData dataWithBytes:jsonBody.data() length:jsonBody.size()];
    [request setHTTPBody:bodyData];

    // Perform a synchronous request via a semaphore over NSURLSession.
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block NSData*             responseData  = nil;
    __block NSError*            responseError = nil;
    __block NSHTTPURLResponse*  httpResponse  = nil;

    NSURLSession* session = [NSURLSession sharedSession];
    NSURLSessionDataTask* task =
        [session dataTaskWithRequest:request
                   completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
            responseData  = data;
            responseError = error;
            httpResponse  = (NSHTTPURLResponse*)response;
            dispatch_semaphore_signal(sema);
        }];
    [task resume];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    if (responseError) {
        throw std::runtime_error(
            std::string("HTTPClient: ") +
            [[responseError localizedDescription] UTF8String]);
    }

    std::string result;
    if (responseData && [responseData length] > 0) {
        result = std::string(reinterpret_cast<const char*>([responseData bytes]),
                             [responseData length]);
    }

    NSInteger statusCode = [httpResponse statusCode];
    if (statusCode < 200 || statusCode >= 300) {
        throw std::runtime_error(
            "HTTPClient: HTTP " + std::to_string(static_cast<int>(statusCode)) +
            " from: " + url + " — " + result);
    }

    return result;
}

} // namespace routing
