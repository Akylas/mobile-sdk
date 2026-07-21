#include "PostProcessEffect.h"

namespace carto {

    PostProcessEffect::PostProcessEffect(const std::string& name, const std::string& fragmentShader) :
        _name(name),
        _fragmentShader(fragmentShader),
        _terrainDepthRequired(false),
        _floatParameters(),
        _mutex()
    {
    }

    PostProcessEffect::~PostProcessEffect() {
    }

    const std::string& PostProcessEffect::getName() const {
        return _name;
    }

    const std::string& PostProcessEffect::getFragmentShader() const {
        return _fragmentShader;
    }

    bool PostProcessEffect::isTerrainDepthRequired() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _terrainDepthRequired;
    }

    void PostProcessEffect::setTerrainDepthRequired(bool required) {
        std::lock_guard<std::mutex> lock(_mutex);
        _terrainDepthRequired = required;
    }

    float PostProcessEffect::getFloatParameter(const std::string& name) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _floatParameters.find(name);
        return it != _floatParameters.end() ? it->second : 0.0f;
    }

    void PostProcessEffect::setFloatParameter(const std::string& name, float value) {
        std::lock_guard<std::mutex> lock(_mutex);
        _floatParameters[name] = value;
    }

    std::map<std::string, float> PostProcessEffect::getFloatParameters() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _floatParameters;
    }

    std::shared_ptr<PostProcessEffect> PostProcessEffect::CreateReliefOutlineEffect() {
        static const std::string reliefOutlineFsh = R"GLSL(#version 100
            precision mediump float;

            uniform sampler2D uColorTex;
            uniform sampler2D uTerrainDepthTex;
            uniform vec2 uInvScreenSize;
            uniform float uIntensity;
            uniform float uOutlineWidth;
            uniform float uDepthThreshold;

            float unpackDepth(vec4 c) {
                return dot(c.rgb, vec3(1.0, 1.0 / 255.0, 1.0 / 65025.0));
            }

            void main(void) {
                vec2 uv = gl_FragCoord.xy * uInvScreenSize;
                vec4 color = texture2D(uColorTex, uv);

                vec2 delta = uInvScreenSize * uOutlineWidth;
                vec4 c0 = texture2D(uTerrainDepthTex, uv);
                float d0 = unpackDepth(c0);
                float dx0 = unpackDepth(texture2D(uTerrainDepthTex, uv - vec2(delta.x, 0.0)));
                float dx1 = unpackDepth(texture2D(uTerrainDepthTex, uv + vec2(delta.x, 0.0)));
                float dy0 = unpackDepth(texture2D(uTerrainDepthTex, uv - vec2(0.0, delta.y)));
                float dy1 = unpackDepth(texture2D(uTerrainDepthTex, uv + vec2(0.0, delta.y)));

                // Depth discontinuities relative to the local depth produce ridge/silhouette lines.
                // Sky pixels unpack to ~1.0, giving strong silhouettes against the sky.
                float dd = max(max(abs(dx0 - d0), abs(dx1 - d0)), max(abs(dy0 - d0), abs(dy1 - d0)));
                float threshold = uDepthThreshold * (0.001 + 0.02 * d0);
                float edge = smoothstep(threshold, threshold * 2.0, dd);

                // Subtle depth-based shading to separate distant ridges (lighter with distance)
                float shade = mix(0.15, 0.75, clamp(d0 * 2.0, 0.0, 1.0));
                vec3 stylized = mix(vec3(1.0), vec3(shade), edge);

                gl_FragColor = vec4(mix(color.rgb, stylized, uIntensity), 1.0);
            }
        )GLSL";

        auto effect = std::make_shared<PostProcessEffect>("relief_outline", reliefOutlineFsh);
        effect->setTerrainDepthRequired(true);
        effect->setFloatParameter("uIntensity", 1.0f);
        effect->setFloatParameter("uOutlineWidth", 1.5f);
        effect->setFloatParameter("uDepthThreshold", 1.0f);
        return effect;
    }
}
