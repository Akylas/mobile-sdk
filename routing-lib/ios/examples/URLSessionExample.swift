// ---------------------------------------------------------------------------
// URLSession integration example for NTValhallaRoutingService (iOS, Swift)
// ---------------------------------------------------------------------------
//
// NTHTTPPostHandler is a synchronous block — bridged to Swift as a closure.
// Since URLSession's default data task API is async we use a semaphore to
// make it synchronous, matching the handler contract.
//
// Call all routing methods off the main thread; e.g. inside a Task { } or
// DispatchQueue.global().async { … }.

import Foundation
// import the routing module, e.g.:
// import ValhallaRouting   (Swift Package)
// or the bridging header if using CocoaPods / manual xcframework

// ---------------------------------------------------------------------------
// Factory — build NTValhallaOnlineRoutingService backed by URLSession
// ---------------------------------------------------------------------------

func makeOnlineServiceWithURLSession(
    baseURL: String = "https://valhalla1.openstreetmap.de",
    profile: String = "pedestrian",
    session: URLSession = .shared
) -> NTValhallaOnlineRoutingService {

    let handler: NTHTTPPostHandler = { url, body, errorPtr in
        guard let nsURL = URL(string: url) else {
            errorPtr?.pointee = NSError(
                domain: "NTRoutingError", code: -1,
                userInfo: [NSLocalizedDescriptionKey: "Invalid URL: \(url)"])
            return nil
        }

        var request = URLRequest(url: nsURL)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.setValue("application/json", forHTTPHeaderField: "Accept")
        request.httpBody = body.data(using: .utf8)

        // Make async URLSession synchronous with a semaphore
        let semaphore = DispatchSemaphore(value: 0)
        var responseBody: String? = nil
        var responseError: Error? = nil

        session.dataTask(with: request) { data, response, error in
            defer { semaphore.signal() }
            if let error {
                responseError = error
                return
            }
            guard let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode) else {
                responseError = NSError(
                    domain: "NTRoutingError", code: (response as? HTTPURLResponse)?.statusCode ?? -1,
                    userInfo: [NSLocalizedDescriptionKey:
                        "HTTP \((response as? HTTPURLResponse)?.statusCode ?? -1) from \(url)"])
                return
            }
            responseBody = data.flatMap { String(data: $0, encoding: .utf8) }
        }.resume()

        semaphore.wait()

        if let err = responseError {
            errorPtr?.pointee = err as NSError
            return nil
        }
        return responseBody
    }

    let service = NTValhallaOnlineRoutingService(baseURL: baseURL, handler: handler)!
    service.profile = profile
    return service
}

// ---------------------------------------------------------------------------
// Usage examples
// ---------------------------------------------------------------------------

/// Online route between two WGS-84 points, result is raw Valhalla JSON.
///
/// Must run off the main thread.
func exampleOnlineRoute() {
    let service = makeOnlineServiceWithURLSession()

    let request = NTRoutingRequest(points: [
        NTLatLon.lat(48.8566, lon: 2.3522),   // Paris
        NTLatLon.lat(48.8738, lon: 2.2950),   // Bois de Boulogne
    ])
    // Optional: override any Valhalla field as JSON
    request.customJSON = #"{"units":"kilometers"}"#

    var error: NSError?
    if let rawJSON = service.calculateRoute(request, error: &error) {
        print(rawJSON)
    } else {
        print("Error: \(error?.localizedDescription ?? "unknown")")
    }
}

/// Offline route via MBTiles — no network needed.
func exampleOfflineRoute(mbtilesPath: String) {
    guard let service = NTValhallaRoutingService(mbTilesPaths: [mbtilesPath]) else { return }
    service.profile = "auto"

    let request = NTRoutingRequest(points: [
        NTLatLon.lat(48.8566, lon: 2.3522),
        NTLatLon.lat(48.8738, lon: 2.2950),
    ])

    var error: NSError?
    if let rawJSON = service.calculateRoute(request, error: &error) {
        print(rawJSON)
    }
}

/// Direct isochrone call — any Valhalla endpoint.
func exampleIsochrone() {
    let service = makeOnlineServiceWithURLSession()

    let body = """
    {
      "locations": [{"lon": 2.3522, "lat": 48.8566}],
      "costing": "pedestrian",
      "contours": [{"time": 10}, {"time": 20}]
    }
    """

    var error: NSError?
    if let rawJSON = service.callRaw("isochrone", jsonBody: body, error: &error) {
        print(rawJSON)
    }
}

/// Map-matching a GPS trace.
func exampleMapMatch() {
    let service = makeOnlineServiceWithURLSession()

    let request = NTRouteMatchingRequest(
        points: [
            NTLatLon.lat(48.8566, lon: 2.3522),
            NTLatLon.lat(48.8600, lon: 2.3600),
            NTLatLon.lat(48.8650, lon: 2.3700),
        ],
        accuracy: 15   // GPS accuracy in metres
    )

    var error: NSError?
    if let rawJSON = service.matchRoute(request, error: &error) {
        print(rawJSON)
    }
}
