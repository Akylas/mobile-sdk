package com.akylas.routing

/**
 * Online Valhalla routing service.
 *
 * Two build modes are supported:
 *
 * 1) Built with ROUTING_WITH_HTTP_CLIENT (recommended):
 *    HTTP transport is handled internally by the native C++ HTTP client.
 *    Use the single-argument constructor `ValhallaOnlineRoutingService(baseURL)`.
 *    No external HTTP handler is required.
 *
 * 2) Built without ROUTING_WITH_HTTP_CLIENT:
 *    HTTP transport is handled by the caller-supplied [HttpPostHandler].
 *    Use `ValhallaOnlineRoutingService(baseURL, handler)`.
 *    The handler may use any HTTP stack (OkHttp, Fuel, Ktor, etc.).
 *
 * All routing methods may block and must be called from a background thread.
 *
 * Results are returned as raw Valhalla JSON strings.
 */
class ValhallaOnlineRoutingService @JvmOverloads constructor(
    baseURL: String,
    private val httpHandler: HttpPostHandler? = null
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
     * When built with ROUTING_WITH_HTTP_CLIENT and no handler was supplied,
     * the native C++ HTTP client is used. Otherwise the supplied [httpHandler]
     * performs the HTTP request.
     *
     * @param endpoint  e.g. "route", "trace_attributes", "isochrone" …
     * @param jsonBody  Pre-built Valhalla JSON request.
     * @return          Raw Valhalla JSON response string.
     * @throws RuntimeException on HTTP or service error.
     */
    fun callRaw(endpoint: String, jsonBody: String): String {
        val url = "${baseURL.trimEnd('/')}/$endpoint"
        return if (httpHandler != null) {
            httpHandler.post(url, jsonBody)
        } else {
            nativeHttpPost(url, jsonBody)
        }
    }

    // -----------------------------------------------------------------------
    // Native (C++ HTTP client) — only available when built with
    // ROUTING_WITH_HTTP_CLIENT=ON.
    // -----------------------------------------------------------------------

    private external fun nativeHttpPost(url: String, body: String): String

    companion object {
        init {
            System.loadLibrary("valhalla_routing")
        }
    }
}
