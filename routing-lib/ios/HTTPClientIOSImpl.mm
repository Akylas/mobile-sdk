#include "network/HTTPClientIOSImpl.h"
#include "exceptions/Exceptions.h"
#include "log/Log.h"

#import <Foundation/Foundation.h>

@interface RoutingURLConnection : NSObject <NSURLSessionDelegate, NSURLSessionTaskDelegate, NSURLSessionDataDelegate>

-(id)init;
-(void)cancel;
-(void)deinit;
-(NSError*)sendSynchronousRequest:(NSURLRequest*)request
               didReceiveResponse:(BOOL(^)(NSURLResponse*))responseHandler
                  didReceiveData:(BOOL(^)(NSData*))dataHandler;

@end

@interface RoutingURLConnection ()

@property(nonatomic, strong) NSURLSession*           session;
@property(nonatomic, strong) NSCondition*            condition;
@property(nonatomic, strong) NSMutableDictionary*    responseHandlers;
@property(nonatomic, strong) NSMutableDictionary*    dataHandlers;

@end

@implementation RoutingURLConnection

-(id)init {
    self = [super init];
    NSURLSessionConfiguration* defaultConfigObject = [NSURLSessionConfiguration defaultSessionConfiguration];
    self.session          = [NSURLSession sessionWithConfiguration:defaultConfigObject delegate:self delegateQueue:nil];
    self.condition        = [[NSCondition alloc] init];
    self.responseHandlers = [[NSMutableDictionary alloc] init];
    self.dataHandlers     = [[NSMutableDictionary alloc] init];
    return self;
}

-(void)cancel {
    [self.session invalidateAndCancel];
}

-(void)deinit {
    [self.session finishTasksAndInvalidate];
}

-(NSError*)sendSynchronousRequest:(NSURLRequest*)request
               didReceiveResponse:(BOOL(^)(NSURLResponse*))responseHandler
                  didReceiveData:(BOOL(^)(NSData*))dataHandler {
    NSURLSessionDataTask* dataTask = [self.session dataTaskWithRequest:request];

    [self.condition lock];
    [self.responseHandlers setObject:responseHandler forKey:dataTask];
    [self.dataHandlers     setObject:dataHandler     forKey:dataTask];
    [self.condition unlock];

    [dataTask resume];

    [self.condition lock];
    while ([self.responseHandlers objectForKey:dataTask]) {
        [self.condition wait];
    }
    [self.condition unlock];

    return dataTask.error;
}

-(void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error {
    [self signalConnectionDidFinishLoading:(NSURLSessionDataTask*)task];
}

-(void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task
willPerformHTTPRedirection:(NSHTTPURLResponse*)response
       newRequest:(NSURLRequest*)request
completionHandler:(void (^)(NSURLRequest*))completionHandler {
    completionHandler(request);
}

-(void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)dataTask
didReceiveResponse:(NSURLResponse*)response
completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler {
    completionHandler(NSURLSessionResponseAllow);

    [self.condition lock];
    BOOL(^responseHandler)(NSURLResponse*) = [self.responseHandlers objectForKey:dataTask];
    [self.condition unlock];

    if (responseHandler(response)) {
        [dataTask cancel];
        [self signalConnectionDidFinishLoading:dataTask];
    }
}

-(void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)dataTask
   didReceiveData:(NSData*)data {
    [self.condition lock];
    BOOL(^dataHandler)(NSData*) = [self.dataHandlers objectForKey:dataTask];
    [self.condition unlock];

    if (dataHandler(data)) {
        [dataTask cancel];
        [self signalConnectionDidFinishLoading:dataTask];
    }
}

-(void)signalConnectionDidFinishLoading:(NSURLSessionDataTask*)dataTask {
    [self.condition lock];
    [self.responseHandlers removeObjectForKey:dataTask];
    [self.dataHandlers     removeObjectForKey:dataTask];
    [self.condition signal];
    [self.condition unlock];
}

@end

namespace routing {

    HTTPClient::IOSImpl::IOSImpl(bool log) :
        _log(log),
        _timeout(-1)
    {
    }

    HTTPClient::IOSImpl::~IOSImpl() {
    }

    void HTTPClient::IOSImpl::setTimeout(int milliseconds) {
        _timeout = milliseconds;
    }

    bool HTTPClient::IOSImpl::makeRequest(const HTTPClient::Request& request,
                                          HeadersFunc headersFn,
                                          DataFunc    dataFn) const {
        NSURL* url = [NSURL URLWithString:[NSString stringWithUTF8String:request.url.c_str()]];

        NSMutableURLRequest* mutableRequest = [[NSMutableURLRequest alloc] init];
        [mutableRequest setURL:url];
        [mutableRequest setHTTPShouldUsePipelining:YES];

        int timeout = _timeout.load();
        if (timeout > 0) {
            [mutableRequest setTimeoutInterval:timeout / 1000.0];
        }

        NSString* method = [NSString stringWithUTF8String:request.method.c_str()];
        [mutableRequest setHTTPMethod:method];

        // Set request headers
        for (auto it = request.headers.begin(); it != request.headers.end(); ++it) {
            NSString* key = [NSString stringWithUTF8String:it->first.c_str()];
            NSString* val = [NSString stringWithUTF8String:it->second.c_str()];
            [mutableRequest addValue:val forHTTPHeaderField:key];
        }

        // Set request body if Content-Type is defined
        if (!request.contentType.empty()) {
            NSData* body = [NSData dataWithBytes:request.body.data() length:request.body.size()];
            [mutableRequest setHTTPBody:body];
        }

        __block BOOL cancel = false;
        BOOL(^handleResponse)(NSURLResponse*) = ^BOOL(NSURLResponse* response) {
            NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
            int statusCode = static_cast<int>([httpResponse statusCode]);

            std::map<std::string, std::string> headers;
            for (NSString* key in [httpResponse allHeaderFields]) {
                NSString* val = [[httpResponse allHeaderFields] objectForKey:key];
                headers[std::string([key UTF8String])] = std::string([val UTF8String]);
            }

            if (!headersFn(statusCode, headers)) {
                cancel = YES;
            }
            return cancel;
        };
        BOOL(^handleData)(NSData*) = ^BOOL(NSData* data) {
            if (!dataFn(reinterpret_cast<const unsigned char*>([data bytes]), [data length])) {
                cancel = YES;
            }
            return cancel;
        };

        RoutingURLConnection* connection = [[RoutingURLConnection alloc] init];
        NSError* error = [connection sendSynchronousRequest:[mutableRequest copy]
                                         didReceiveResponse:handleResponse
                                            didReceiveData:handleData];
        if (error) {
            [connection cancel];
            NSString* description = [error localizedDescription];
            throw NetworkException(std::string([description UTF8String]), request.url);
        }
        [connection deinit];

        return cancel == NO;
    }

} // namespace routing
