/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_POSTPROCESSEFFECT_H_
#define _CARTO_POSTPROCESSEFFECT_H_

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace carto {
    /**
     * A full-screen post-processing effect. When attached to the map via
     * MapRenderer::setPostProcessEffect, the map is rendered into an offscreen buffer
     * and the effect fragment shader produces the final screen output.
     *
     * The fragment shader must be GLSL ES 1.00 (#version 100) source. The following
     * uniforms are provided by the renderer:
     * - sampler2D uColorTex: the rendered map frame (premultiplied alpha).
     * - sampler2D uTerrainDepthTex: packed terrain depth buffer (only if terrain depth is required;
     *   RGB = 24-bit fixed point linear depth 0..1 relative to the far plane, A = terrain coverage).
     *   Use dot(rgb, vec3(1.0, 1.0/255.0, 1.0/65025.0)) to unpack.
     * - vec2 uInvScreenSize: 1/width, 1/height of the screen in pixels.
     * - float uNear, uFar: view frustum distances (internal units).
     * - float uTime: seconds since the effect was attached.
     * Additionally all float parameters set via setFloatParameter are available as uniforms.
     * Screen texture coordinates can be computed as gl_FragCoord.xy * uInvScreenSize.
     *
     * Note: this class is experimental and may change or even be removed in future SDK versions.
     */
    class PostProcessEffect {
    public:
        /**
         * Constructs a post-process effect from a fragment shader.
         * @param name The name of the effect (used for shader caching, should be unique).
         * @param fragmentShader The GLSL ES 1.00 fragment shader source.
         */
        PostProcessEffect(const std::string& name, const std::string& fragmentShader);
        virtual ~PostProcessEffect();

        /**
         * Returns the name of the effect.
         * @return The name of the effect.
         */
        const std::string& getName() const;
        /**
         * Returns the fragment shader source of the effect.
         * @return The fragment shader source.
         */
        const std::string& getFragmentShader() const;

        /**
         * Returns true if the effect needs the terrain depth pre-pass (uTerrainDepthTex).
         * @return True if terrain depth is rendered for the effect. The default is false.
         */
        bool isTerrainDepthRequired() const;
        /**
         * Sets whether the effect needs the terrain depth pre-pass.
         * @param required True if the terrain depth should be rendered for the effect.
         */
        void setTerrainDepthRequired(bool required);

        /**
         * Returns the value of a float parameter.
         * @param name The name of the parameter.
         * @return The value of the parameter, or 0 if not set.
         */
        float getFloatParameter(const std::string& name) const;
        /**
         * Sets a float parameter. The parameter is exposed to the fragment shader as a uniform.
         * @param name The name of the parameter (must be a valid GLSL identifier).
         * @param value The new value for the parameter.
         */
        void setFloatParameter(const std::string& name, float value);

        /**
         * Returns all float parameters. Internal method.
         * @return The map of all parameters.
         */
        std::map<std::string, float> getFloatParameters() const;

        /**
         * Creates a built-in 'relief outline' effect: renders the terrain as dark contour
         * lines on a light background (PeakFinder-style). Requires terrain to be enabled.
         * Parameters: uIntensity (0..1 blend with the original map, default 1),
         * uOutlineWidth (line width in pixels, default 1.5), uDepthThreshold (edge sensitivity, default 1).
         * @return The relief outline effect.
         */
        static std::shared_ptr<PostProcessEffect> CreateReliefOutlineEffect();

    private:
        const std::string _name;
        const std::string _fragmentShader;

        bool _terrainDepthRequired;
        std::map<std::string, float> _floatParameters;

        mutable std::mutex _mutex;
    };
}

#endif
