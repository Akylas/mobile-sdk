package com.akylas.routing.examples

import com.akylas.routing.LatLon
import com.akylas.routing.RouteMatchingRequest
import com.akylas.routing.RoutingRequest
import com.akylas.routing.ValhallaOnlineRoutingService
import com.akylas.routing.ValhallaRoutingService
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import java.io.IOException
import java.util.concurrent.TimeUnit

// ---------------------------------------------------------------------------
// OkHttp integration example for ValhallaOnlineRoutingService
// ---------------------------------------------------------------------------

private val JSON_MEDIA_TYPE = "application/json; charset=utf-8".toMediaType()

/**
 * Build an OkHttp-backed [ValhallaOnlineRoutingService].
 *
 * [ValhallaOnlineRoutingService.HttpPostHandler] is a synchronous `fun interface` —
 * OkHttp's [okhttp3.Call.execute] fits naturally.
 *
 * Remember to call routing methods off the main thread (e.g. inside a coroutine
 * with Dispatchers.IO, or in a background executor).
 */
fun buildOnlineServiceWithOkHttp(
    baseURL: String = "https://valhalla1.openstreetmap.de",
    profile: String = "pedestrian",
    okHttpClient: OkHttpClient = defaultOkHttpClient()
): ValhallaOnlineRoutingService {

    val handler = ValhallaOnlineRoutingService.HttpPostHandler { url, body ->
        val request = Request.Builder()
            .url(url)
            .post(body.toRequestBody(JSON_MEDIA_TYPE))
            .header("Accept", "application/json")
            .build()

        okHttpClient.newCall(request).execute().use { response ->
            if (!response.isSuccessful) {
                throw IOException("Valhalla HTTP error ${response.code}: ${response.message}")
            }
            response.body?.string()
                ?: throw IOException("Empty response body from $url")
        }
    }

    return ValhallaOnlineRoutingService(baseURL, handler).also { it.profile = profile }
}

private fun defaultOkHttpClient(): OkHttpClient = OkHttpClient.Builder()
    .connectTimeout(10, TimeUnit.SECONDS)
    .readTimeout(30, TimeUnit.SECONDS)
    .writeTimeout(30, TimeUnit.SECONDS)
    .build()

// ---------------------------------------------------------------------------
// Usage examples
// ---------------------------------------------------------------------------

/**
 * Example — online route between two WGS-84 points, result is raw JSON.
 *
 * Run this on a background thread / inside Dispatchers.IO.
 */
fun exampleOnlineRoute() {
    val service = buildOnlineServiceWithOkHttp()

    val request = RoutingRequest(
        listOf(
            LatLon(lat = 48.8566, lon = 2.3522),   // Paris
            LatLon(lat = 48.8738, lon = 2.2950)    // Bois de Boulogne
        )
    ).setParameter("costing_options", """{"pedestrian":{"use_roads":0.5}}""")

    val rawJson: String = service.calculateRoute(request)
    println(rawJson)
}

/**
 * Example — offline route using the native MBTiles-backed service.
 *
 * Uses JNI; MBTiles file must exist at the given path.
 */
fun exampleOfflineRoute(mbtilesPath: String) {
    ValhallaRoutingService(listOf(mbtilesPath)).use { service ->
        service.profile = "auto"

        val request = RoutingRequest(
            listOf(
                LatLon(lat = 48.8566, lon = 2.3522),
                LatLon(lat = 48.8738, lon = 2.2950)
            )
        )

        val rawJson: String = service.calculateRoute(request)
        println(rawJson)
    }
}

/**
 * Example — call any Valhalla endpoint directly (e.g. isochrone).
 */
fun exampleIsochrone() {
    val service = buildOnlineServiceWithOkHttp()

    val isochroneBody = """
        {
          "locations": [{"lon": 2.3522, "lat": 48.8566}],
          "costing": "pedestrian",
          "contours": [{"time": 10}, {"time": 20}]
        }
    """.trimIndent()

    val rawJson: String = service.callRaw("isochrone", isochroneBody)
    println(rawJson)
}

/**
 * Example — map-matching a GPS trace.
 */
fun exampleMapMatch() {
    val service = buildOnlineServiceWithOkHttp()

    val request = RouteMatchingRequest(
        points = listOf(
            LatLon(lat = 48.8566, lon = 2.3522),
            LatLon(lat = 48.8600, lon = 2.3600),
            LatLon(lat = 48.8650, lon = 2.3700)
        ),
        accuracy = 15f   // GPS accuracy in metres
    )

    val rawJson: String = service.matchRoute(request)
    println(rawJson)
}
