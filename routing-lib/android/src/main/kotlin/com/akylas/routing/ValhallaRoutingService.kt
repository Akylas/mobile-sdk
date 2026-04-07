package com.akylas.routing

/**
 * Offline Valhalla routing service backed by MBTiles databases.
 *
 * All routing results are returned as raw Valhalla JSON strings.
 * Parsing of individual fields is the responsibility of the caller.
 *
 * All routing methods may perform disk I/O and must be called from a
 * background thread (not the main/UI thread).
 */
class ValhallaRoutingService(paths: List<String> = emptyList()) {

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
     * @throws RuntimeException if the file cannot be opened.
     */
    fun addMBTilesPath(path: String): Unit = nativeAddMBTilesPath(nativePtr, path)

    // -----------------------------------------------------------------------
    // Profile
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

    // -----------------------------------------------------------------------
    // Routing API — all return raw Valhalla JSON
    // -----------------------------------------------------------------------

    /**
     * Calculate a route.
     * @param request  Routing request built via [RoutingRequest].
     * @return Raw Valhalla route JSON string.
     * @throws RuntimeException on routing failure.
     */
    fun calculateRoute(request: RoutingRequest): String =
        nativeCalculateRoute(nativePtr, request.toJSON())

    /**
     * Match a GPS trace to the road network.
     * @param request  Route-matching request built via [RouteMatchingRequest].
     * @return Raw Valhalla trace_attributes JSON string.
     * @throws RuntimeException on matching failure.
     */
    fun matchRoute(request: RouteMatchingRequest): String =
        nativeMatchRoute(nativePtr, request.toJSON())

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

    fun close(): Unit = nativeDestroy(nativePtr)

    protected fun finalize(): Unit = close()

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
    private external fun nativeCalculateRoute(ptr: Long, requestJSON: String): String
    private external fun nativeMatchRoute(ptr: Long, requestJSON: String): String
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
 */
class RoutingRequest(private val points: List<LatLon>) {

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
        for ((k, v) in customParams) {
            sb.append(",\"").append(k.replace("\"", "\\\"")).append("\":").append(v)
        }
        sb.append('}')
        return sb.toString()
    }
}

/**
 * Route-matching request builder.
 */
class RouteMatchingRequest(private val points: List<LatLon>,
                           private val accuracy: Float = 0f) {

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
        for ((k, v) in customParams) {
            sb.append(",\"").append(k.replace("\"", "\\\"")).append("\":").append(v)
        }
        sb.append('}')
        return sb.toString()
    }
}
