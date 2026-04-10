package com.akylas.routing

/**
 * Offline Valhalla routing service backed by MBTiles databases.
 *
 * All routing results are returned as raw Valhalla JSON strings.
 * Parsing of individual fields is the responsibility of the caller.
 *
 * All routing methods may perform disk I/O and must be called from a
 * background thread (not the main/UI thread).
 *
 * Databases are opened lazily on the first [callRaw] invocation and closed
 * automatically when the request completes (or when all concurrent requests finish).
 */
class ValhallaRoutingService(paths: List<String> = emptyList()) : AutoCloseable {

    private val nativePtr: Long

    init {
        nativePtr = nativeCreate()
        for (path in paths) {
            addMBTilesPath(path)
        }
    }

    // -----------------------------------------------------------------------
    // Data sources
    // -----------------------------------------------------------------------

    /**
     * Add an MBTiles database by file path.
     * The database will be opened on the first [callRaw] and closed when idle.
     */
    fun addMBTilesPath(path: String): Unit = nativeAddMBTilesPath(nativePtr, path)

    // -----------------------------------------------------------------------
    // Profile (costing model)
    //
    // The profile is automatically injected as "costing" into every [callRaw]
    // request body that does not already contain that key.
    // -----------------------------------------------------------------------

    /** Valhalla costing model, e.g. "pedestrian", "auto", "bicycle". */
    var profile: String
        get() = nativeGetProfile(nativePtr)
        set(value) = nativeSetProfile(nativePtr, value)

    // -----------------------------------------------------------------------
    // Valhalla configuration (dot-delimited key paths)
    // -----------------------------------------------------------------------

    /**
     * Get a Valhalla configuration value as a JSON string.
     * @return JSON string of the value, or null if not found.
     */
    fun getConfigurationParameter(key: String): String? =
        nativeGetConfigParam(nativePtr, key)

    /**
     * Set a Valhalla configuration value.
     * @param jsonValue JSON-encoded value, e.g. "0.5", "\"residential\"", "true".
     */
    fun setConfigurationParameter(key: String, jsonValue: String): Unit =
        nativeSetConfigParam(nativePtr, key, jsonValue)

    // -----------------------------------------------------------------------
    // Locale support
    // -----------------------------------------------------------------------

    /**
     * Register a Valhalla locale JSON blob for narrative instructions.
     */
    fun addLocale(key: String, json: String): Unit = nativeAddLocale(nativePtr, key, json)

    /**
     * Parse a valhalla response shape into [[lon,lat]...]
     */
    fun parseShape(shape: String): String = nativeParseShape(nativePtr, shape)

    // -----------------------------------------------------------------------
    // Routing API — all return raw Valhalla JSON
    // -----------------------------------------------------------------------

    /**
     * Calculate a route.
     *
     * The service [profile] is injected as "costing" if [request] does not
     * set one explicitly. Equivalent to `callRaw("route", request.toJSON())`.
     *
     * @param request  Routing request built via [RoutingRequest].
     * @return Raw Valhalla route JSON string.
     */
    fun calculateRoute(request: RoutingRequest): String =
        callRaw("route", request.toJSON())

    /**
     * Match a GPS trace to the road network.
     *
     * Equivalent to `callRaw("trace_attributes", request.toJSON())`.
     *
     * @param request  Route-matching request built via [RouteMatchingRequest].
     * @return Raw Valhalla trace_attributes JSON string.
     */
    fun matchRoute(request: RouteMatchingRequest): String =
        callRaw("trace_attributes", request.toJSON())

    /**
     * Call any Valhalla API endpoint directly.
     *
     * @param endpoint  e.g. "route", "trace_attributes", "isochrone", "matrix", "locate" …
     * @param jsonBody  Pre-built Valhalla request JSON string.
     * @return          Raw Valhalla JSON response string.
     * @throws RuntimeException on failure.
     */
    fun callRaw(endpoint: String, jsonBody: String): String =
        nativeCallRaw(nativePtr, endpoint, jsonBody)

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    override fun close(): Unit = nativeDestroy(nativePtr)

    // -----------------------------------------------------------------------
    // JNI declarations
    // -----------------------------------------------------------------------

    private external fun nativeCreate(): Long
    private external fun nativeDestroy(ptr: Long)
    private external fun nativeAddMBTilesPath(ptr: Long, path: String)
    private external fun nativeGetProfile(ptr: Long): String
    private external fun nativeSetProfile(ptr: Long, profile: String)
    private external fun nativeGetConfigParam(ptr: Long, key: String): String?
    private external fun nativeSetConfigParam(ptr: Long, key: String, jsonValue: String)
    private external fun nativeAddLocale(ptr: Long, key: String, json: String)
    private external fun nativeParseShape(ptr: Long, shape: String): String
    private external fun nativeCallRaw(ptr: Long, endpoint: String, jsonBody: String): String

    companion object {
        init {
            System.loadLibrary("valhalla_routing")
        }
    }
}

// ---------------------------------------------------------------------------
// RoutingRequest — convenience builder for Kotlin
// ---------------------------------------------------------------------------

/**
 * A single WGS-84 waypoint.
 */
data class LatLon(val lat: Double, val lon: Double)

/**
 * Routing request builder.
 *
 * Set [profile] to override the service-level costing model for this request.
 */
class RoutingRequest(private val points: List<LatLon>) {

    /** Optional costing model override (e.g. "auto", "pedestrian", "bicycle"). */
    var profile: String? = null

    private val customParams = mutableMapOf<String, String>()

    /**
     * Set a Valhalla request parameter by dot-delimited key.
     * @param jsonValue JSON-encoded value string.
     */
    fun setParameter(key: String, jsonValue: String): RoutingRequest {
        customParams[key] = jsonValue
        return this
    }

    internal fun toJSON(): String {
        val sb = StringBuilder()
        sb.append("{\"locations\":[")
        points.forEachIndexed { idx, ll ->
            if (idx > 0) sb.append(',')
            sb.append("{\"lon\":").append(ll.lon).append(",\"lat\":").append(ll.lat).append('}')
        }
        sb.append(']')
        // Include costing if a profile is set on the request.
        profile?.let { p -> sb.append(",\"costing\":\"").append(p).append('"') }
        for ((k, v) in customParams) {
            sb.append(",\"").append(k.replace("\"", "\\\"")).append("\":").append(v)
        }
        sb.append('}')
        return sb.toString()
    }
}

/**
 * Route-matching request builder.
 *
 * Set [profile] to override the service-level costing model for this request.
 */
class RouteMatchingRequest(
    private val points: List<LatLon>,
    private val accuracy: Float = 0f
) {

    /** Optional costing model override (e.g. "auto", "pedestrian", "bicycle"). */
    var profile: String? = null

    private val customParams = mutableMapOf<String, String>()

    fun setParameter(key: String, jsonValue: String): RouteMatchingRequest {
        customParams[key] = jsonValue
        return this
    }

    internal fun toJSON(): String {
        val sb = StringBuilder()
        sb.append("{\"shape\":[")
        points.forEachIndexed { idx, ll ->
            if (idx > 0) sb.append(',')
            sb.append("{\"lon\":").append(ll.lon).append(",\"lat\":").append(ll.lat).append('}')
        }
        sb.append("],\"shape_match\":\"map_snap\"")
        if (accuracy > 0f) sb.append(",\"gps_accuracy\":").append(accuracy)
        // Include costing if a profile is set on the request.
        profile?.let { p -> sb.append(",\"costing\":\"").append(p).append('"') }
        for ((k, v) in customParams) {
            sb.append(",\"").append(k.replace("\"", "\\\"")).append("\":").append(v)
        }
        sb.append('}')
        return sb.toString()
    }
}
