#include "SkyRenderer.h"
#include "components/Options.h"
#include "terrain/ElevationManager.h"
#include "components/LightOptions.h"
#include "components/SkyOptions.h"
#include "components/StyleEnvironment.h"
#include "components/TerrainOptions.h"
#include "graphics/ViewState.h"
#include "renderers/utils/GLResourceManager.h"
#include "renderers/utils/Shader.h"
#include "utils/Const.h"
#include "utils/Log.h"

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif

#include <cglib/mat.h>

namespace massif {

    SkyRenderer::SkyRenderer(const Options& options) :
        _shader(),
        _shaderSource(),
        _shaderFailed(false),
        _a_coord(0),
        _u_invMVPMat(-1),
        _u_sunDir(-1),
        _u_sunColor(-1),
        _u_skyColor(-1),
        _u_horizonColor(-1),
        _u_groundColor(-1),
        _u_horizonBlend(-1),
        _u_sunIntensity(-1),
        _u_sunDisc(-1),
        _u_time(-1),
        _u_zoom(-1),
        _u_cameraHeight(-1),
        _u_resolution(-1),
        _u_fogColor(-1),
        _u_fogBlend(-1),
        _u_fogHorizon(-1),
        _startTime(std::chrono::steady_clock::now()),
        _glResourceManager(),
        _options(options)
    {
    }

    SkyRenderer::~SkyRenderer() {
    }

    void SkyRenderer::onSurfaceCreated(const std::shared_ptr<GLResourceManager>& resourceManager) {
        _glResourceManager = resourceManager;
        _shader.reset();
        _shaderSource.clear();
        _shaderFailed = false;
    }

    void SkyRenderer::onSurfaceDestroyed() {
        _shader.reset();
        _shaderSource.clear();
        _shaderFailed = false;
        _glResourceManager.reset();
    }

    bool SkyRenderer::updateShader() {
        std::shared_ptr<SkyOptions> skyOptions = _options.getSkyOptions();
        std::string source = skyOptions ? skyOptions->getShaderSource() : std::string();
        if (_shader && _shaderSource == source) {
            return true;
        }

        // A custom source that failed to compile once must not be retried every frame.
        if (_shaderFailed && _shaderSource == source) {
            return static_cast<bool>(_shader);
        }

        _shaderSource = source;
        _shaderFailed = false;

        std::string body = source.empty() ? SKY_FRAGMENT_SHADER_BUILTIN : source;
        std::shared_ptr<Shader> shader = _glResourceManager->create<Shader>("sky", SKY_VERTEX_SHADER, SKY_FRAGMENT_SHADER_PREFIX + body + SKY_FRAGMENT_SHADER_MAIN);
        if (shader->getProgId() == 0 && !source.empty()) {
            Log::Error("SkyRenderer::updateShader: Custom sky shader failed to compile, falling back to the built-in shader");
            _shaderFailed = true;
            shader = _glResourceManager->create<Shader>("sky", SKY_VERTEX_SHADER, SKY_FRAGMENT_SHADER_PREFIX + SKY_FRAGMENT_SHADER_BUILTIN + SKY_FRAGMENT_SHADER_MAIN);
        }
        if (shader->getProgId() == 0) {
            _shader.reset();
            return false;
        }

        _shader = shader;
        GLuint progId = _shader->getProgId();
        _a_coord = _shader->getAttribLoc("a_coord");
        _u_invMVPMat = glGetUniformLocation(progId, "u_invMVPMat");
        _u_sunDir = glGetUniformLocation(progId, "u_sunDir");
        _u_sunColor = glGetUniformLocation(progId, "u_sunColor");
        _u_skyColor = glGetUniformLocation(progId, "u_skyColor");
        _u_horizonColor = glGetUniformLocation(progId, "u_horizonColor");
        _u_groundColor = glGetUniformLocation(progId, "u_groundColor");
        _u_horizonBlend = glGetUniformLocation(progId, "u_horizonBlend");
        _u_sunIntensity = glGetUniformLocation(progId, "u_sunIntensity");
        _u_sunDisc = glGetUniformLocation(progId, "u_sunDisc");
        _u_time = glGetUniformLocation(progId, "u_time");
        _u_zoom = glGetUniformLocation(progId, "u_zoom");
        _u_cameraHeight = glGetUniformLocation(progId, "u_cameraHeight");
        _u_resolution = glGetUniformLocation(progId, "u_resolution");
        _u_fogColor = glGetUniformLocation(progId, "u_fogColor");
        _u_fogBlend = glGetUniformLocation(progId, "u_fogBlend");
        _u_fogHorizon = glGetUniformLocation(progId, "u_fogHorizon");
        return true;
    }

    // Measurement switch: debug.massif.skyclip 0 draws the sky over the whole screen again, which
    // is what it did before the quad was clipped to the horizon. Read once (Android only).
#ifdef __ANDROID__
    bool SkyRenderer::isHorizonClipEnabled() {
        static const bool enabled = [] {
            char property[PROP_VALUE_MAX] = { 0 };
            return !(__system_property_get("debug.massif.skyclip", property) > 0 && property[0] == '0');
        }();
        return enabled;
    }
#else
    bool SkyRenderer::isHorizonClipEnabled() {
        return true;
    }
#endif

    bool SkyRenderer::onDrawFrame(const ViewState& viewState) {
        std::shared_ptr<SkyOptions> skyOptions = _options.getSkyOptions();
        if (!skyOptions || !skyOptions->isEnabled() || !_glResourceManager) {
            return false;
        }

        // With 3D terrain the sky can be exposed by a peak even when the horizon plane is not
        // in view, so the cheap horizon test is only used when the terrain is flat.
        if (!viewState.isSkyVisible()) {
            std::shared_ptr<TerrainOptions> terrainOptions = _options.getTerrainOptions();
            if (!terrainOptions || !terrainOptions->isEnabled()) {
                return false;
            }
        }

        if (!updateShader()) {
            return false;
        }

        std::shared_ptr<LightOptions> lightOptions = _options.getLightOptions();
        cglib::vec3<float> sunDir(0.0f, 0.0f, 1.0f);
        Color sunColor(255, 255, 255, 255);
        float sunIntensity = 1.0f;
        if (lightOptions) {
            sunDir = lightOptions->getSunDirection();
            sunColor = lightOptions->getSunColor();
            sunIntensity = lightOptions->getSunIntensity();
        }

        // The terrain fog, resolved and lit exactly as the ground fog is, so the haze the map
        // fades into carries on into the sky instead of stopping in a band at the skyline.
        ResolvedLighting lighting = resolveLighting(lightOptions, StyleEnvironment());
        ResolvedFog fog = resolveFog(_options.getTerrainOptions(), StyleEnvironment(), lighting);
        float fogBlend = fog.active() ? static_cast<float>(skyOptions->getFogBlend() * Const::DEG_TO_RAD) : 0.0f;
        // Haze starts fading at the angle of the highest terrain the view can hold, not at the
        // mathematical horizon - see docs/rendering/08-lighting-sky-fog.md.
        float fogHorizonSetting = skyOptions->getFogHorizon();
        float fogHorizon = (fogHorizonSetting > 0 ? static_cast<float>(fogHorizonSetting * Const::DEG_TO_RAD) : 0.0f);
        if (fogBlend > 0.0f && fogHorizonSetting < 0) {
            if (std::shared_ptr<TerrainOptions> terrainOptions = _options.getTerrainOptions()) {
                if (terrainOptions->isEnabled()) {
                    if (std::shared_ptr<ElevationManager> elevationManager = terrainOptions->getElevationManager()) {
                        double minZ = 0, maxZ = 0;
                        elevationManager->getDisplayHeightRange(viewState.getFocusPos()(1), minZ, maxZ);
                        double above = maxZ - viewState.getCameraPos()(2);
                        double distance = fog.distance * Const::WORLD_SIZE / Const::EARTH_CIRCUMFERENCE;
                        if (above > 0 && distance > 0) {
                            // Capped at half the blend: the auto angle comes from the HIGHEST ground
                            // the elevation manager has seen, which is a whole massif away from what
                            // is on screen, and left uncapped it lifts the full-strength band over
                            // most of the visible sky.
                            fogHorizon = std::min(static_cast<float>(std::atan2(above, distance)), fogBlend * 0.5f);
                        }
                    }
                }
            }
        }

        Color skyColor = skyOptions->getSkyColor();
        Color horizonColor = skyOptions->getHorizonColor();
        Color groundColor = skyOptions->getGroundColor();

        cglib::mat4x4<float> invMVPMat = cglib::inverse(viewState.getRTEModelviewProjectionMat());

        glUseProgram(_shader->getProgId());
        if (_u_invMVPMat >= 0) {
            glUniformMatrix4fv(_u_invMVPMat, 1, GL_FALSE, invMVPMat.data());
        }
        if (_u_sunDir >= 0) {
            glUniform3fv(_u_sunDir, 1, sunDir.data());
        }
        if (_u_sunColor >= 0) {
            glUniform4f(_u_sunColor, sunColor.getR() / 255.0f, sunColor.getG() / 255.0f, sunColor.getB() / 255.0f, sunColor.getA() / 255.0f);
        }
        if (_u_skyColor >= 0) {
            glUniform4f(_u_skyColor, skyColor.getR() / 255.0f, skyColor.getG() / 255.0f, skyColor.getB() / 255.0f, skyColor.getA() / 255.0f);
        }
        if (_u_horizonColor >= 0) {
            glUniform4f(_u_horizonColor, horizonColor.getR() / 255.0f, horizonColor.getG() / 255.0f, horizonColor.getB() / 255.0f, horizonColor.getA() / 255.0f);
        }
        if (_u_groundColor >= 0) {
            glUniform4f(_u_groundColor, groundColor.getR() / 255.0f, groundColor.getG() / 255.0f, groundColor.getB() / 255.0f, groundColor.getA() / 255.0f);
        }
        if (_u_horizonBlend >= 0) {
            glUniform1f(_u_horizonBlend, static_cast<float>(skyOptions->getHorizonBlend() * Const::DEG_TO_RAD));
        }
        if (_u_sunIntensity >= 0) {
            glUniform1f(_u_sunIntensity, sunIntensity);
        }
        if (_u_sunDisc >= 0) {
            glUniform1f(_u_sunDisc, skyOptions->isSunDiscEnabled() ? 1.0f : 0.0f);
        }
        if (_u_time >= 0) {
            glUniform1f(_u_time, std::chrono::duration_cast<std::chrono::duration<float> >(std::chrono::steady_clock::now() - _startTime).count());
        }
        if (_u_zoom >= 0) {
            glUniform1f(_u_zoom, viewState.getZoom());
        }
        if (_u_cameraHeight >= 0) {
            glUniform1f(_u_cameraHeight, static_cast<float>(viewState.getCameraPos()(2) * Const::EARTH_CIRCUMFERENCE / Const::WORLD_SIZE));
        }
        if (_u_resolution >= 0) {
            glUniform2f(_u_resolution, static_cast<float>(viewState.getWidth()), static_cast<float>(viewState.getHeight()));
        }
        if (_u_fogColor >= 0) {
            glUniform4f(_u_fogColor, fog.color.getR() / 255.0f, fog.color.getG() / 255.0f, fog.color.getB() / 255.0f, fog.color.getA() / 255.0f);
        }
        if (_u_fogBlend >= 0) {
            glUniform1f(_u_fogBlend, fogBlend);
        }
        if (_u_fogHorizon >= 0) {
            glUniform1f(_u_fogHorizon, fogHorizon);
        }

        // Start the quad at the horizon plus a margin for the fog band - everything below is drawn
        // over anyway (docs/rendering/08-lighting-sky-fog.md). Not applied when the terrain path
        // draws the sky although the flat horizon says it is not visible.
        float quadBottom = -1.0f;
        if (viewState.isSkyVisible() && isHorizonClipEnabled()) {
            quadBottom = std::max(-1.0f, viewState.getSkyHorizonNDC() - SKY_HORIZON_MARGIN);
        }
        const float quadCoords[8] = { -1, quadBottom, 1, quadBottom, -1, 1, 1, 1 };

        glDisable(GL_CULL_FACE);
        glEnableVertexAttribArray(_a_coord);
        glVertexAttribPointer(_a_coord, 2, GL_FLOAT, GL_FALSE, 0, quadCoords);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableVertexAttribArray(_a_coord);
        glEnable(GL_CULL_FACE);

        GLContext::CheckGLError("SkyRenderer::onDrawFrame");
        return true;
    }

    const float SkyRenderer::QUAD_COORDS[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    const float SkyRenderer::SKY_HORIZON_MARGIN = 0.35f;

    const std::string SkyRenderer::SKY_VERTEX_SHADER = R"GLSL(
        #version 100
        attribute vec2 a_coord;
        uniform mat4 u_invMVPMat;
        varying vec3 v_rayDir;
        void main() {
            // The modelview matrix is relative to the eye, so unprojecting a near-plane point
            // gives the world-space view ray directly.
            vec4 nearPos = u_invMVPMat * vec4(a_coord, -1.0, 1.0);
            v_rayDir = nearPos.xyz / nearPos.w;
            gl_Position = vec4(a_coord, 1.0, 1.0);
        }
    )GLSL";

    const std::string SkyRenderer::SKY_FRAGMENT_SHADER_PREFIX = R"GLSL(
        #version 100
        #ifdef GL_FRAGMENT_PRECISION_HIGH
        precision highp float;
        #else
        precision mediump float;
        #endif
        varying vec3 v_rayDir;
        uniform vec3 u_sunDir;
        uniform vec4 u_sunColor;
        uniform vec4 u_skyColor;
        uniform vec4 u_horizonColor;
        uniform vec4 u_groundColor;
        uniform float u_horizonBlend;
        uniform float u_sunIntensity;
        uniform float u_sunDisc;
        uniform float u_time;
        uniform float u_zoom;
        uniform float u_cameraHeight;
        uniform vec2 u_resolution;
        uniform vec4 u_fogColor;  // the terrain fog, already lit by the sun; a = strength at the horizon
        uniform float u_fogBlend; // elevation angle (radians) the fog fades out over, measured from u_fogHorizon
        uniform float u_fogHorizon; // elevation angle (radians) the haze is still full at - the skyline, not the horizon

        // The sky's share of the terrain fog for a view ray: full at the horizon, gone by
        // u_fogBlend. Cubed so the haze hugs the horizon and clears quickly with height instead
        // of greying half the sky.
        float fogAmount(vec3 rayDir) {
            if (u_fogBlend <= 0.0) {
                return 0.0;
            }
            float elevation = asin(clamp(normalize(rayDir).z, -1.0, 1.0));
            float t = clamp(1.0 - max(elevation - u_fogHorizon, 0.0) / u_fogBlend, 0.0, 1.0);
            return t * t * t * u_fogColor.a;
        }
    )GLSL";

    // The default appearance: a horizon-to-zenith gradient, a broad glow around the sun and a
    // sun disc of about 1.5 degrees. Everything below the horizon takes the ground colour,
    // which is transparent by default so the map/clear colour shows through.
    const std::string SkyRenderer::SKY_FRAGMENT_SHADER_BUILTIN = R"GLSL(
        vec4 skyColor(vec3 rayDir) {
            float elevation = asin(clamp(rayDir.z, -1.0, 1.0));
            if (elevation < 0.0) {
                // BELOW the mathematical horizon, and that is exactly the band the drawn ground
                // stops short of: the terrain ends at the view distance, well before the horizon,
                // and everything between the two is this ray. Returning the ground colour alone -
                // transparent by default - left the map's clear colour there, so the hazed ground
                // met it along a hard line, which is the "fog does not reach the sky" edge seen far
                // in the distance. Anything down there is beyond the last tile, so it is haze:
                // fogAmount is at full strength below the horizon by construction.
                return mix(u_groundColor, vec4(u_fogColor.rgb, 1.0), fogAmount(rayDir));
            }
            float t = u_horizonBlend > 0.0 ? clamp(elevation / u_horizonBlend, 0.0, 1.0) : 1.0;
            vec4 color = mix(u_horizonColor, u_skyColor, t);
            // Blend the ground fog into the sky above the horizon, so the two meet in a gradient
            // rather than at a hard skyline.
            color.rgb = mix(color.rgb, u_fogColor.rgb, fogAmount(rayDir));
            if (u_sunDisc > 0.5) {
                // Chord length between the two unit vectors, which is the angle in radians to
                // within 1% over the few degrees that matter here - and unlike acos/pow it keeps
                // full precision right at the centre of the disc.
                float d = length(rayDir - u_sunDir);
                float disc = 1.0 - smoothstep(0.0040, 0.0050, d);   // the sun is about 0.5 degrees across
                float glow = exp(-d * d / 0.0012) * 0.45;
                float halo = exp(-d * d / 0.0220) * 0.12;
                // The glow tints towards the sun colour instead of adding to it: an additive
                // glow saturates a bright sky to white long before it reaches the sun.
                color.rgb = mix(color.rgb, u_sunColor.rgb, clamp((halo + glow) * u_sunIntensity, 0.0, 1.0));
                color.rgb += u_sunColor.rgb * u_sunIntensity * disc;
                color.a = max(color.a, disc * u_sunColor.a);
            }
            return color;
        }
    )GLSL";

    const std::string SkyRenderer::SKY_FRAGMENT_SHADER_MAIN = R"GLSL(
        void main() {
            vec4 color = skyColor(normalize(v_rayDir));
            color = clamp(color, 0.0, 1.0);
            gl_FragColor = vec4(color.rgb * color.a, color.a);
        }
    )GLSL";
}
