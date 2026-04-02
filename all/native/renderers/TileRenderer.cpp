#include "TileRenderer.h"
#include "components/Options.h"
#include "components/ThreadWorker.h"
#include "graphics/ViewState.h"
#include "projections/ProjectionSurface.h"
#include "projections/PlanarProjectionSurface.h"
#include "renderers/MapRenderer.h"
#include "renderers/drawdatas/TileDrawData.h"
#include "renderers/utils/GLResourceManager.h"
#include "renderers/utils/VTRenderer.h"
#include "layers/HillshadeRasterTileLayer.h"
#include "utils/Const.h"
#include "utils/Log.h"
#include "utils/Const.h"

#include <vt/Label.h>
#include <vt/LabelCuller.h>
#include <vt/TileTransformer.h>
#include <vt/GLExtensions.h>

#include <cmath>

#include <cglib/mat.h>

namespace carto {
    
    TileRenderer::TileRenderer() :
        _mapRenderer(),
        _options(),
        _tileTransformer(),
        _vtRenderer(),
        _interactionMode(false),
        _layerBlendingSpeed(1.0f),
        _labelBlendingSpeed(1.0f),
        _labelOrder(0),
        _buildingOrder(1),
        _rasterFilterMode(vt::RasterFilterMode::BILINEAR),
        _normalMapLightingShader(LIGHTING_SHADER_NORMALMAP),
        _normalMapShadowColor(0, 0, 0, 255),
        _normalMapAccentColor(0, 0, 0, 255),
        _normalMapHighlightColor(255, 255, 255, 255),
        _rendererLayerFilter(),
        _clickHandlerLayerFilter(),
        _horizontalLayerOffset(0),
        _viewDir(0, 0, 0),
        _mainLightDir(0, 0, 0),
        _normalLightDir(0, 0, 0),
        _normalIlluminationMapRotationEnabled(false),
        _normalIlluminationDirection(0,0,0),
        _mapRotation(0),
        _hillshadeMethod(HillshadeMethod::STANDARD),
        _hillshadeExaggeration(0.5f),
        _tiles(),
        _mutex()
    {
    }
    
    TileRenderer::~TileRenderer() {
    }
    
    void TileRenderer::setComponents(const std::weak_ptr<Options>& options, const std::weak_ptr<MapRenderer>& mapRenderer) {
        std::lock_guard<std::mutex> lock(_mutex);
        _options = options;
        _mapRenderer = mapRenderer;
        _vtRenderer.reset();
    }

    std::shared_ptr<vt::TileTransformer> TileRenderer::getTileTransformer() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _tileTransformer;
    }

    void TileRenderer::setTileTransformer(const std::shared_ptr<vt::TileTransformer>& tileTransformer) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_tileTransformer != tileTransformer) {
            _vtRenderer.reset();
        }
        _tileTransformer = tileTransformer;
    }
    
    void TileRenderer::setInteractionMode(bool enabled) {
        std::lock_guard<std::mutex> lock(_mutex);
        _interactionMode = enabled;
    }
    
    void TileRenderer::setLayerBlendingSpeed(float speed) {
        std::lock_guard<std::mutex> lock(_mutex);
        _layerBlendingSpeed = speed;
    }

    void TileRenderer::setLabelBlendingSpeed(float speed) {
        std::lock_guard<std::mutex> lock(_mutex);
        _labelBlendingSpeed = speed;
    }

    void TileRenderer::setLabelOrder(int order) {
        std::lock_guard<std::mutex> lock(_mutex);
        _labelOrder = order;
    }
    
    void TileRenderer::setBuildingOrder(int order) {
        std::lock_guard<std::mutex> lock(_mutex);
        _buildingOrder = order;
    }

    void TileRenderer::setRasterFilterMode(vt::RasterFilterMode filterMode) {
        std::lock_guard<std::mutex> lock(_mutex);
        _rasterFilterMode = filterMode;
    }

    void TileRenderer::setNormalMapShadowColor(const Color& color) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalMapShadowColor = color;
    }

    void TileRenderer::setNormalMapHighlightColor(const Color& color) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalMapHighlightColor = color;
    }
    void TileRenderer::setNormalMapAccentColor(const Color& color) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalMapAccentColor = color;
    }
    void TileRenderer::setNormalMapLightingShader(const std::string& shader) {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string newValue = shader;
        if (newValue.length() == 0) {
            newValue = LIGHTING_SHADER_NORMALMAP;
        }
        if (newValue != _normalMapLightingShader) {
            _normalMapLightingShader = newValue;
            _vtRenderer.reset();
        }
    }
    void TileRenderer::setNormalIlluminationDirection(MapVec direction) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalIlluminationDirection = direction;
    }

    void TileRenderer::setNormalIlluminationMapRotationEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalIlluminationMapRotationEnabled = enabled;
    }

    void TileRenderer::setHillshadeMethod(int method) {
        std::lock_guard<std::mutex> lock(_mutex);
        _hillshadeMethod = method;
    }

    void TileRenderer::setHillshadeExaggeration(float exaggeration) {
        std::lock_guard<std::mutex> lock(_mutex);
        _hillshadeExaggeration = exaggeration;
    }

    void TileRenderer::setRendererLayerFilter(const std::optional<std::regex>& filter) {
        std::lock_guard<std::mutex> lock(_mutex);
        _rendererLayerFilter = filter;
    }

    void TileRenderer::setClickHandlerLayerFilter(const std::optional<std::regex>& filter) {
        std::lock_guard<std::mutex> lock(_mutex);
        _clickHandlerLayerFilter = filter;
    }

    void TileRenderer::offsetLayerHorizontally(double offset) {
        std::lock_guard<std::mutex> lock(_mutex);
        _horizontalLayerOffset += offset;
    }
    
    bool TileRenderer::onDrawFrame(float deltaSeconds, const ViewState& viewState) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (!initializeRenderer()) {
            return false;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer();
        if (!tileRenderer) {
            return false;
        }

        cglib::mat4x4<double> modelViewMat = viewState.getModelviewMat() * cglib::translate4_matrix(cglib::vec3<double>(_horizontalLayerOffset, 0, 0));
        tileRenderer->setViewState(vt::ViewState(viewState.getProjectionMat(), modelViewMat, viewState.getZoom(), viewState.getRotation(), viewState.getTilt(), viewState.getAspectRatio(), viewState.getNormalizedResolution()));
        tileRenderer->setInteractionMode(_interactionMode);
        tileRenderer->setRasterFilterMode(_rasterFilterMode);
        tileRenderer->setLayerBlendingSpeed(_layerBlendingSpeed);
        tileRenderer->setLabelBlendingSpeed(_labelBlendingSpeed);
        tileRenderer->setRendererLayerFilter(_rendererLayerFilter);


        _mapRotation = viewState.getRotation();
        _viewDir = cglib::unit(viewState.getFocusPosNormal());
        if (auto options = _options.lock()) {
            MapPos internalFocusPos = viewState.getProjectionSurface()->calculateMapPos(viewState.getFocusPos());
            _mainLightDir = cglib::vec3<float>::convert(cglib::unit(viewState.getProjectionSurface()->calculateVector(internalFocusPos, options->getMainLightDirection())));
            MapVec normalIlluminationDir = options->getMainLightDirection();
            if (_normalIlluminationDirection != MapVec(0,0,0)) {
                normalIlluminationDir = _normalIlluminationDirection;
            }
            if (_normalIlluminationMapRotationEnabled) {
                double y = normalIlluminationDir.getY();
                double x = normalIlluminationDir.getX();
                double azimuthal = ((x > 0) ? acos(y) : -acos(y)) * Const::RAD_TO_DEG - _mapRotation;
                double sin = std::sin(azimuthal * Const::DEG_TO_RAD);
                double cos = std::cos(azimuthal * Const::DEG_TO_RAD);
                normalIlluminationDir = MapVec(sin, cos, normalIlluminationDir.getZ());
            }

            _normalLightDir = cglib::vec3<float>::convert(cglib::unit(viewState.getProjectionSurface()->calculateVector(internalFocusPos, normalIlluminationDir)));
        }

        bool refresh = false;
        try {
            refresh = tileRenderer->startFrame(deltaSeconds * 3);

            tileRenderer->renderGeometry(true, false);
            if (_labelOrder == 0) {
                tileRenderer->renderLabels(true, false);
            }
            if (_buildingOrder == 0) {
                tileRenderer->renderGeometry(false, true);
            }
            if (_labelOrder == 0) {
                tileRenderer->renderLabels(false, true);
            }
        }
        catch (const std::exception& ex) {
            Log::Errorf("TileRenderer::onDrawFrame: Rendering failed: %s", ex.what());
        }
    
        // Reset GL state to the expected state
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        GLContext::CheckGLError("TileRenderer::onDrawFrame");
        return refresh;
    }
    
    bool TileRenderer::onDrawFrame3D(float deltaSeconds, const ViewState& viewState) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (!_vtRenderer) {
            return false;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer();
        if (!tileRenderer) {
            return false;
        }

        bool refresh = false;
        try {
            if (_labelOrder == 1) {
                tileRenderer->renderLabels(true, false);
            }
            if (_buildingOrder == 1) {
                tileRenderer->renderGeometry(false, true);
            }
            if (_labelOrder == 1) {
                tileRenderer->renderLabels(false, true);
            }

            refresh = tileRenderer->endFrame();
        }
        catch (const std::exception& ex) {
            Log::Errorf("TileRenderer::onDrawFrame3D: Rendering failed: %s", ex.what());
        }

        // Reset GL state to the expected state
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        GLContext::CheckGLError("TileRenderer::onDrawFrame3D");
        return refresh;
    }
    
    bool TileRenderer::cullLabels(vt::LabelCuller& culler, const ViewState& viewState) {
        std::shared_ptr<vt::GLTileRenderer> tileRenderer;
        cglib::mat4x4<double> modelViewMat;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (_vtRenderer) {
                tileRenderer = _vtRenderer->getTileRenderer();
            }
            modelViewMat = viewState.getModelviewMat() * cglib::translate4_matrix(cglib::vec3<double>(_horizontalLayerOffset, 0, 0));
        }

        if (!tileRenderer) {
            return false;
        }
        culler.setViewState(vt::ViewState(viewState.getProjectionMat(), modelViewMat, viewState.getZoom(),
viewState.getRotation(), viewState.getTilt(), viewState.getAspectRatio(), viewState.getNormalizedResolution()));

        try {
            tileRenderer->cullLabels(culler);
        }
        catch (const std::exception& ex) {
            Log::Errorf("TileRenderer::cullLabels: Culling failed: %s", ex.what());
            return false;
        }
        return true;
    }
    
    bool TileRenderer::refreshTiles(const std::vector<std::shared_ptr<TileDrawData> >& drawDatas) {
        std::lock_guard<std::mutex> lock(_mutex);

        std::map<vt::TileId, std::shared_ptr<const vt::Tile> > tiles;
        for (const std::shared_ptr<TileDrawData>& drawData : drawDatas) {
            tiles[drawData->getVTTileId()] = drawData->getVTTile();
        }

        bool changed = (tiles != _tiles) || (_horizontalLayerOffset != 0);
        if (!changed) {
            return false;
        }

        if (_vtRenderer) {
            if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer()) {
                if (_horizontalLayerOffset != 0) {
                    tileRenderer->teleportVisibleTiles((int)std::round(_horizontalLayerOffset / Const::WORLD_SIZE), 0);
                }
                tileRenderer->setVisibleTiles(tiles);
            }
        }
        _tiles = std::move(tiles);
        _horizontalLayerOffset = 0;
        return true;
    }

    void TileRenderer::calculateRayIntersectedElements(const cglib::ray3<double>& ray, const ViewState& viewState, float radius, std::vector<vt::GLTileRenderer::GeometryIntersectionInfo>& results) const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_vtRenderer) {
            return;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer();
        if (!tileRenderer) {
            return;
        }

        tileRenderer->setClickHandlerLayerFilter(_clickHandlerLayerFilter);

        std::vector<cglib::ray3<double> > rays = { ray };
        tileRenderer->findGeometryIntersections(rays, radius, radius, true, false, results);
        if (_labelOrder == 0) {
            tileRenderer->findLabelIntersections(rays, radius, true, false, results);
        }
        if (_buildingOrder == 0) {
            tileRenderer->findGeometryIntersections(rays, radius, radius, false, true, results);
        }
        if (_labelOrder == 0) {
            tileRenderer->findLabelIntersections(rays, radius, false, true, results);
        }
    }
        
    void TileRenderer::calculateRayIntersectedElements3D(const cglib::ray3<double>& ray, const ViewState& viewState, float radius, std::vector<vt::GLTileRenderer::GeometryIntersectionInfo>& results) const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_vtRenderer) {
            return;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer();
        if (!tileRenderer) {
            return;
        }

        std::vector<cglib::ray3<double> > rays = { ray };
        if (_labelOrder == 1) {
            tileRenderer->findLabelIntersections(rays, radius, true, false, results);
        }
        if (_buildingOrder == 1) {
            tileRenderer->findGeometryIntersections(rays, radius, radius, false, true, results);
        }
        if (_labelOrder == 1) {
            tileRenderer->findLabelIntersections(rays, radius, false, true, results);
        }
    }

    void TileRenderer::calculateRayIntersectedBitmaps(const cglib::ray3<double>& ray, const ViewState& viewState, std::vector<vt::GLTileRenderer::BitmapIntersectionInfo>& results) const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_vtRenderer) {
            return;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer();
        if (!tileRenderer) {
            return;
        }

        std::vector<cglib::ray3<double> > rays = { ray };
        tileRenderer->findBitmapIntersections(rays, results);
    }

    Color TileRenderer::evaluateColorFunc(const vt::ColorFunction& colorFunc, const ViewState& viewState) {
        cglib::mat4x4<double> modelViewMat = viewState.getModelviewMat();
        vt::ViewState vtViewState(viewState.getProjectionMat(), modelViewMat, viewState.getZoom(),
viewState.getRotation(), viewState.getTilt(), viewState.getAspectRatio(), viewState.getNormalizedResolution());
        return Color(colorFunc(vtViewState).value());
    }

    bool TileRenderer::initializeRenderer() {
        if (_vtRenderer && _vtRenderer->isValid()) {
            return true;
        }

        std::shared_ptr<MapRenderer> mapRenderer = _mapRenderer.lock();
        if (!mapRenderer) {
            return false; // safety check, should never happen
        }

        Log::Debug("TileRenderer: Initializing renderer");
        _vtRenderer = mapRenderer->getGLResourceManager()->create<VTRenderer>(_tileTransformer);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer()) {
            tileRenderer->setVisibleTiles(_tiles);

            if (!std::dynamic_pointer_cast<PlanarProjectionSurface>(mapRenderer->getProjectionSurface())) {
                vt::GLTileRenderer::LightingShader lightingShader2D(true, LIGHTING_SHADER_2D, [this](GLuint shaderProgram, const vt::ViewState& viewState) {
                    glUniform3fv(glGetUniformLocation(shaderProgram, "u_viewDir"), 1, _viewDir.data());
                });
                tileRenderer->setLightingShader2D(lightingShader2D);
            }

            vt::GLTileRenderer::LightingShader lightingShader3D(true, LIGHTING_SHADER_3D, [this](GLuint shaderProgram, const vt::ViewState& viewState) {
                if (auto options = _options.lock()) {
                    const Color& ambientLightColor = options->getAmbientLightColor();
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_ambientColor"), ambientLightColor.getR() / 255.0f, ambientLightColor.getG() / 255.0f, ambientLightColor.getB() / 255.0f, ambientLightColor.getA() / 255.0f);
                    const Color& mainLightColor = options->getMainLightColor();
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_lightColor"), mainLightColor.getR() / 255.0f, mainLightColor.getG() / 255.0f, mainLightColor.getB() / 255.0f, mainLightColor.getA() / 255.0f);
                    glUniform3fv(glGetUniformLocation(shaderProgram, "u_lightDir"), 1, _mainLightDir.data());
                    glUniform3fv(glGetUniformLocation(shaderProgram, "u_viewDir"), 1, _viewDir.data());
                }
            });
            tileRenderer->setLightingShader3D(lightingShader3D);

            vt::GLTileRenderer::LightingShader lightingShaderNormalMap(false, _normalMapLightingShader, [this](GLuint shaderProgram, const vt::ViewState& viewState) {
                    float shadowAlpha = _normalMapShadowColor.getA() / 255.0f;
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_shadowColor"), _normalMapShadowColor.getR() * shadowAlpha / 255.0f, _normalMapShadowColor.getG() * shadowAlpha / 255.0f, _normalMapShadowColor.getB() * shadowAlpha / 255.0f,  shadowAlpha);
                    float accentAlpha = _normalMapAccentColor.getA() / 255.0f;
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_accentColor"), _normalMapAccentColor.getR() * accentAlpha / 255.0f, _normalMapAccentColor.getG() * accentAlpha / 255.0f, _normalMapAccentColor.getB() * accentAlpha / 255.0f, accentAlpha);
                    float highlightAlpha = _normalMapHighlightColor.getA() / 255.0f;
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_highlightColor"), _normalMapHighlightColor.getR() * highlightAlpha / 255.0f, _normalMapHighlightColor.getG() * highlightAlpha / 255.0f, _normalMapHighlightColor.getB() * highlightAlpha / 255.0f,  highlightAlpha);
                    glUniform3fv(glGetUniformLocation(shaderProgram, "u_lightDir"), 1, _normalLightDir.data() );
                    glUniform1i(glGetUniformLocation(shaderProgram, "u_method"), (_hillshadeMethod));
                    glUniform1f(glGetUniformLocation(shaderProgram, "u_exaggeration"), _hillshadeExaggeration);
            });
            tileRenderer->setLightingShaderNormalMap(lightingShaderNormalMap);
        }

        return _vtRenderer && _vtRenderer->isValid();
    }

    const std::string TileRenderer::LIGHTING_SHADER_2D = R"GLSL(
        uniform vec3 u_viewDir;
        vec4 applyLighting(lowp vec4 color, mediump vec3 normal) {
            mediump float lighting = max(0.0, dot(normal, u_viewDir)) * 0.5 + 0.5;
            return vec4(color.rgb * lighting, color.a);
        }
    )GLSL";

    const std::string TileRenderer::LIGHTING_SHADER_3D = R"GLSL(
        uniform vec4 u_ambientColor;
        uniform vec4 u_lightColor;
        uniform vec3 u_lightDir;
        uniform vec3 u_viewDir;
        vec4 applyLighting(lowp vec4 color, mediump vec3 normal, highp_opt float height, bool sideVertex) {
            if (sideVertex) {
                lowp vec3 dimmedColor = color.rgb * (1.0 - 0.5 / (1.0 + height * height));
                mediump vec3 lighting = max(0.0, dot(normal, u_lightDir)) * u_lightColor.rgb + u_ambientColor.rgb;
                return vec4(dimmedColor.rgb * lighting, color.a);
            } else {
                mediump float lighting = max(0.0, dot(normal, u_viewDir)) * 0.5 + 0.5;
                return vec4(color.rgb * lighting, color.a);
            }
        }
    )GLSL";

    const std::string TileRenderer::LIGHTING_SHADER_NORMALMAP = R"GLSL(
        uniform vec4 u_shadowColor;
        uniform vec4 u_highlightColor;
        uniform vec4 u_accentColor;
        uniform vec3 u_lightDir;
        uniform int u_method;
        uniform float u_exaggeration;

        #define PI 3.141592653589793
        #define STANDARD 0
        #define COMBINED 1
        #define IGOR 2
        #define MULTIDIRECTIONAL 3
        #define BASIC 4

        float get_aspect(vec2 deriv) {
            return deriv.x != 0.0 ? atan(deriv.y, -deriv.x) : PI / 2.0 * (deriv.y > 0.0 ? 1.0 : -1.0);
        }

        // Based on GDALHillshadeIgorAlg()
        vec4 igor_hillshade(vec2 deriv, vec3 lightDir) {
            float aspect = get_aspect(deriv);
            // Convert light direction to azimuth
            float azimuth = atan(lightDir.y, lightDir.x) + PI;
            float slope_strength = atan(length(deriv)) * 2.0/PI;
            float aspect_strength = 1.0 - abs(mod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);
            float shadow_strength = slope_strength * aspect_strength;
            float highlight_strength = slope_strength * (1.0-aspect_strength);
            return u_shadowColor * shadow_strength + u_highlightColor * highlight_strength;
        }

        // MapLibre's legacy hillshade algorithm
        vec4 standard_hillshade(vec2 deriv, vec3 lightDir) {
            // Convert light direction to azimuth
            float azimuth = atan(lightDir.y, lightDir.x) + PI;

            // We multiply the slope by an arbitrary z-factor of 0.625
            float slope = atan(0.625 * length(deriv));
            float aspect = get_aspect(deriv);

            float intensity = u_exaggeration;

            // Scale the slope exponentially based on intensity
            float base = 1.875 - intensity * 1.75;
            float maxValue = 0.5 * PI;
            float scaledSlope = intensity != 0.5 ? ((pow(base, slope) - 1.0) / (pow(base, maxValue) - 1.0)) * maxValue : slope;

            // The accent color is calculated with the cosine of the slope
            float accent = cos(scaledSlope);
            vec4 accent_color = (1.0 - accent) * u_accentColor * clamp(intensity * 2.0, 0.0, 1.0);
            
            // Shade color based on aspect and azimuth
            float shade = abs(mod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);
            vec4 shade_color = mix(u_shadowColor, u_highlightColor, shade) * sin(scaledSlope) * clamp(intensity * 2.0, 0.0, 1.0);
            
            return accent_color * (1.0 - shade_color.a) + shade_color;
        }

        // Based on GDALHillshadeAlg()
        vec4 basic_hillshade(vec2 deriv, vec3 lightDir) {
            float azimuth = atan(lightDir.y, lightDir.x) + PI;
            float altitude = asin(clamp(lightDir.z, -1.0, 1.0));
            
            float cos_az = cos(azimuth);
            float sin_az = sin(azimuth);
            float cos_alt = cos(altitude);
            float sin_alt = sin(altitude);

            float cang = (sin_alt - (deriv.y*cos_az*cos_alt - deriv.x*sin_az*cos_alt)) / sqrt(1.0 + dot(deriv, deriv));

            float shade = clamp(cang, 0.0, 1.0);
            if(shade > 0.5) {
                return u_highlightColor * (2.0*shade - 1.0);
            } else {
                return u_shadowColor * (1.0 - 2.0*shade);
            }
        }

        // Multidirectional hillshade (simplified to single light for now)
        vec4 multidirectional_hillshade(vec2 deriv, vec3 lightDir) {
            // For now, just use basic hillshade with the main light
            // In the future, this could be extended to support multiple lights
            return basic_hillshade(deriv, lightDir);
        }

        // Based on GDALHillshadeCombinedAlg()
        vec4 combined_hillshade(vec2 deriv, vec3 lightDir) {
            float azimuth = atan(lightDir.y, lightDir.x) + PI;
            float altitude = asin(clamp(lightDir.z, -1.0, 1.0));
            
            float cos_az = cos(azimuth);
            float sin_az = sin(azimuth);
            float cos_alt = cos(altitude);
            float sin_alt = sin(altitude);

            float cang = acos(clamp((sin_alt - (deriv.y*cos_az*cos_alt - deriv.x*sin_az*cos_alt)) / sqrt(1.0 + dot(deriv, deriv)), -1.0, 1.0));

            cang = clamp(cang, 0.0, PI/2.0);

            float shade = cang * atan(length(deriv)) * 4.0/PI/PI;
            float highlight = (PI/2.0-cang) * atan(length(deriv)) * 4.0/PI/PI;

            return u_shadowColor*shade + u_highlightColor*highlight;
        }

        vec4 applyLighting(lowp vec4 color, mediump vec3 normal, mediump vec3 surfaceNormal, mediump float intensity) {
            // Extract derivatives from normal vector
            // The normal is already in tangent space where z points up
            // For a flat surface, normal would be (0, 0, 1)
            // The derivatives represent the slope in x and y directions
            vec2 deriv = vec2(-normal.x / max(normal.z, 0.001), -normal.y / max(normal.z, 0.001));
            
            // Apply exaggeration to derivatives
            deriv *= u_exaggeration * 2.0;
            
            vec4 hillshadeColor;
            if (u_method == BASIC) {
                hillshadeColor = basic_hillshade(deriv, u_lightDir);
            } else if (u_method == COMBINED) {
                hillshadeColor = combined_hillshade(deriv, u_lightDir);
            } else if (u_method == IGOR) {
                hillshadeColor = igor_hillshade(deriv, u_lightDir);
            } else if (u_method == MULTIDIRECTIONAL) {
                hillshadeColor = multidirectional_hillshade(deriv, u_lightDir);
            } else {
                // STANDARD (default)
                hillshadeColor = standard_hillshade(deriv, u_lightDir);
            }
            
            return hillshadeColor * color * intensity;
        }
    )GLSL";

}
