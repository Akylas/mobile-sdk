package com.akylas.routing

/**
 * Online Valhalla routing service.
 *
 * HTTP transport is handled by the caller-supplied [HttpPostHandler] so that
 * this class works with any HTTP stack (OkHttp, Fuel, Ktor, etc.).
 *
 * All routing methods may block and must be called from a background thread.
 *
 * Results are returned as raw Valhalla JSON strings.
 */
class ValhallaOnlineRoutingService(
    baseURL: String,
    private val httpHandler: HttpPostHandler
) {
    /** Synchronous HTTP POST: given URL and body, return response body or throw. */
    fun interface HttpPostHandler {
        fun post(url: String, body: String): String
    }

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------

    @Volatile
    var baseURL: String = baseURL.trimEnd('/')

    @Volatile
    var profile: String = "pedestrian"

    // -----------------------------------------------------------------------
    // Routing API — all return raw Valhalla JSON
    // -----------------------------------------------------------------------

    /**
     * Calculate a route. POSTs to `{baseURL}/route` and returns raw JSON.
     */
    fun calculateRoute(request: RoutingRequest): String =
        callRaw("route", request.toJSON())

    /**
     * Match a GPS trace. POSTs to `{baseURL}/trace_attributes` and returns raw JSON.
     */
    fun matchRoute(request: RouteMatchingRequest): String =
        callRaw("trace_attributes", request.toJSON())

    /**
     * Call any Valhalla API endpoint directly.
     *
     * @param endpoint  e.g. "route", "trace_attributes", "isochrone" …
     * @param jsonBody  Pre-built Valhalla JSON request.
     * @return          Raw Valhalla JSON response string.
     * @throws RuntimeException on HTTP or service error.
     */
    fun callRaw(endpoint: String, jsonBody: String): String {
        val url = "${baseURL}/$endpoint"
        return httpHandler.post(url, jsonBody)
    }
}
