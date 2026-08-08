#include "PostProcessEffect.h"

namespace carto {

    PostProcessEffect::PostProcessEffect(const std::string& name, const std::string& fragmentShader) :
        _name(name),
        _fragmentShader(fragmentShader),
        _terrainDepthRequired(false),
        _floatParameters(),
        _colorParameters(),
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

    Color PostProcessEffect::getColorParameter(const std::string& name) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _colorParameters.find(name);
        return it != _colorParameters.end() ? it->second : Color(0, 0, 0, 0);
    }

    void PostProcessEffect::setColorParameter(const std::string& name, const Color& color) {
        std::lock_guard<std::mutex> lock(_mutex);
        _colorParameters[name] = color;
    }

    std::map<std::string, float> PostProcessEffect::getFloatParameters() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _floatParameters;
    }

    std::map<std::string, Color> PostProcessEffect::getColorParameters() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _colorParameters;
    }

    std::shared_ptr<PostProcessEffect> PostProcessEffect::CreateReliefOutlineEffect() {
        static const std::string reliefOutlineFsh = R"GLSL(#version 100
            #ifdef GL_FRAGMENT_PRECISION_HIGH
            precision highp float;
            #else
            precision mediump float;
            #endif

            uniform sampler2D uColorTex;
            uniform sampler2D uTerrainDepthTex;
            uniform vec2 uInvScreenSize;
            uniform vec2 uProjInvScale;
            uniform float uFar;
            uniform float uIntensity;
            uniform float uOutlineWidth;
            uniform float uHorizonBoost;
            uniform float uDepthThreshold;
            uniform float uCreaseStrength;
            uniform float uDepthTexelSize;
            uniform float uGrazingFloor;
            uniform float uDistanceFade;
            uniform float uHaze;
            uniform vec4 uInkColor;
            uniform vec4 uPaperColor;

            float unpackDepth(vec4 c) {
                return dot(c.rgb, vec3(1.0, 1.0 / 255.0, 1.0 / 65025.0));
            }

            // Eye-space position of a pixel from the packed linear depth.
            vec3 eyePos(vec2 uv, float depth) {
                vec2 ndc = uv * 2.0 - 1.0;
                return vec3(ndc * uProjInvScale, -1.0) * depth * uFar;
            }

            void main(void) {
                vec2 uv = gl_FragCoord.xy * uInvScreenSize;
                vec4 color = texture2D(uColorTex, uv);

                vec4 c0 = texture2D(uTerrainDepthTex, uv);
                float d0 = unpackDepth(c0);

                // One width for the terrain-against-terrain lines, everywhere. Widening them with
                // distance instead (the obvious reading of "the horizon is bolder") smears the
                // far ranges into a solid band: up there the ridges are a pixel apart, so every
                // pixel is inside some line. What is bold in a panorama is the SKY silhouette,
                // and that gets its own, wider test below.
                // Never narrower than uDepthTexelSize screen pixels: the terrain depth runs at
                // half resolution with nearest filtering, so a narrower step samples the same
                // texel twice and every comparison below degenerates.
                vec2 delta = uInvScreenSize * max(uOutlineWidth, uDepthTexelSize);
                vec2 skyDelta = uInvScreenSize * max(uOutlineWidth * (1.0 + uHorizonBoost), uDepthTexelSize);
                vec4 cx0 = texture2D(uTerrainDepthTex, uv - vec2(delta.x, 0.0));
                vec4 cx1 = texture2D(uTerrainDepthTex, uv + vec2(delta.x, 0.0));
                vec4 cy0 = texture2D(uTerrainDepthTex, uv - vec2(0.0, delta.y));
                vec4 cy1 = texture2D(uTerrainDepthTex, uv + vec2(0.0, delta.y));
                float dx0 = unpackDepth(cx0);
                float dx1 = unpackDepth(cx1);
                float dy0 = unpackDepth(cy0);
                float dy1 = unpackDepth(cy1);

                // The local surface, from the four neighbours. Two things below need it: a
                // surface seen edge-on legitimately changes depth fast from pixel to pixel, and a
                // fold has to be told apart from a merely oblique slope.
                vec3 p0 = eyePos(uv, d0);
                vec3 tx0 = eyePos(uv - vec2(delta.x, 0.0), dx0) - p0;
                vec3 tx1 = eyePos(uv + vec2(delta.x, 0.0), dx1) - p0;
                vec3 ty0 = eyePos(uv - vec2(0.0, delta.y), dy0) - p0;
                vec3 ty1 = eyePos(uv + vec2(0.0, delta.y), dy1) - p0;
                // Two samples that landed on the same depth texel give a zero tangent, and
                // normalizing that is undefined - it painted the whole near field grey.
                float minLength = 1.0e-4 * d0 * uFar;
                bool tangentsValid = length(tx1) > minLength && length(ty1) > minLength;
                float grazing = 1.0;
                if (tangentsValid) {
                    vec3 surfaceNormal = normalize(cross(tx1, ty1));
                    grazing = abs(dot(normalize(-p0), surfaceNormal));
                }

                // Silhouette: the line belongs to the NEARER side of a depth break, so only a
                // neighbour FURTHER away counts. Testing the absolute difference draws the same
                // ridge twice, once on each side, which at the horizon merges into a smear.
                // The threshold is relative to the depth, or the far half of the view draws
                // no line at all - and it is relaxed where the surface is seen EDGE-ON, because
                // there the depth runs away between neighbouring pixels without anything being
                // in front of anything: flat ground at its own horizon drew a solid black band.
                float behind = max(max(dx0 - d0, dx1 - d0), max(dy0 - d0, dy1 - d0));
                float threshold = uDepthThreshold * (0.0008 + 0.02 * d0) / max(grazing, uGrazingFloor);
                float edge = smoothstep(threshold, threshold * 2.0, behind);
                // Terrain-against-terrain lines fade with distance so that the horizon - the sky
                // silhouette below, which does not fade - is the boldest line in the frame.
                edge *= mix(1.0, uDistanceFade, d0);
                // ...and terrain against the sky always is one (coverage, not depth: a sky pixel
                // is at the far plane, which the relative threshold above would forgive). This is
                // the horizon line, and it is the one that is drawn wide.
                float skyNeighbour = 1.0 - min(
                    min(texture2D(uTerrainDepthTex, uv - vec2(skyDelta.x, 0.0)).a, texture2D(uTerrainDepthTex, uv + vec2(skyDelta.x, 0.0)).a),
                    min(texture2D(uTerrainDepthTex, uv - vec2(0.0, skyDelta.y)).a, texture2D(uTerrainDepthTex, uv + vec2(0.0, skyDelta.y)).a));
                edge = max(edge, skyNeighbour * c0.a);

                // Ridges and valleys: the two tangent directions away from this pixel point
                // straight apart on a flat surface (dot -1) and fold together over a crest.
                // Done on eye positions rather than on depth, so a merely oblique slope - which
                // is most of a panorama - does not read as a fold.
                float cover = min(min(cx0.a, cx1.a), min(cy0.a, cy1.a)) * c0.a;
                if (uCreaseStrength > 0.0 && cover > 0.0) {
                    float fold = 0.0;
                    if (length(tx0) > minLength && length(tx1) > minLength) {
                        fold = max(fold, 1.0 + dot(normalize(tx0), normalize(tx1)));
                    }
                    if (length(ty0) > minLength && length(ty1) > minLength) {
                        fold = max(fold, 1.0 + dot(normalize(ty0), normalize(ty1)));
                    }
                    // Same reasoning as the silhouette threshold: an edge-on surface folds in
                    // projection without folding in the world.
                    edge = max(edge, smoothstep(0.05, 0.4, fold) * uCreaseStrength * grazing * mix(1.0, uDistanceFade, d0));
                }

                // Aerial perspective: the shaded surface fades into the paper with distance, so
                // the far ranges read as pale outlines and the near ground keeps its shading.
                vec3 shaded = mix(color.rgb, uPaperColor.rgb, uHaze * d0 * c0.a);
                vec3 stylized = mix(shaded, uInkColor.rgb, edge * uInkColor.a);

                gl_FragColor = vec4(mix(color.rgb, stylized, uIntensity), 1.0);
            }
        )GLSL";

        auto effect = std::make_shared<PostProcessEffect>("relief_outline", reliefOutlineFsh);
        effect->setTerrainDepthRequired(true);
        effect->setFloatParameter("uIntensity", 1.0f);
        effect->setFloatParameter("uOutlineWidth", 1.2f);
        effect->setFloatParameter("uHorizonBoost", 2.5f);
        effect->setFloatParameter("uDepthThreshold", 1.0f);
        effect->setFloatParameter("uCreaseStrength", 0.6f);
        effect->setFloatParameter("uDepthTexelSize", 2.0f); // TerrainRenderer::BUFFER_DOWNSCALE
        effect->setFloatParameter("uGrazingFloor", 0.15f);
        effect->setFloatParameter("uDistanceFade", 0.45f);
        effect->setFloatParameter("uHaze", 0.75f);
        effect->setColorParameter("uInkColor", Color(20, 20, 24, 255));
        effect->setColorParameter("uPaperColor", Color(255, 255, 255, 255));
        return effect;
    }
}
