#include "SkyRenderer.h"
#include "components/Options.h"
#include "components/LightOptions.h"
#include "components/SkyOptions.h"
#include "components/StyleEnvironment.h"
#include "components/TerrainOptions.h"
#include "graphics/ViewState.h"
#include "renderers/utils/GLResourceManager.h"
#include "renderers/utils/Shader.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <cglib/mat.h>

namespace carto {

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
        return true;
    }

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

        glDisable(GL_CULL_FACE);
        glEnableVertexAttribArray(_a_coord);
        glVertexAttribPointer(_a_coord, 2, GL_FLOAT, GL_FALSE, 0, QUAD_COORDS);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableVertexAttribArray(_a_coord);
        glEnable(GL_CULL_FACE);

        GLContext::CheckGLError("SkyRenderer::onDrawFrame");
        return true;
    }

    const float SkyRenderer::QUAD_COORDS[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };

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
        uniform float u_fogBlend; // elevation angle (radians) where the fog has faded out of the sky; 0 = no fog

        // The sky's share of the terrain fog for a view ray: full at the horizon, gone by
        // u_fogBlend. Cubed so the haze hugs the horizon and clears quickly with height instead
        // of greying half the sky.
        float fogAmount(vec3 rayDir) {
            if (u_fogBlend <= 0.0) {
                return 0.0;
            }
            float elevation = asin(clamp(normalize(rayDir).z, -1.0, 1.0));
            float t = clamp(1.0 - max(elevation, 0.0) / u_fogBlend, 0.0, 1.0);
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
                return u_groundColor;
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
