#include "BackgroundRenderer.h"
#include "components/Layers.h"
#include "components/Options.h"
#include "components/StyleEnvironment.h"
#include "components/TerrainOptions.h"
#include "components/FogOptions.h"
#include "graphics/Color.h"
#include "graphics/Bitmap.h"
#include "graphics/ViewState.h"
#include "layers/Layer.h"
#include "renderers/utils/GLResourceManager.h"
#include "renderers/utils/Shader.h"
#include "renderers/utils/Texture.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <algorithm>
#include <cmath>

#include <cglib/mat.h>

namespace massif {

    BackgroundRenderer::BackgroundRenderer(const Options& options, const Layers& layers) :
        _backgroundBitmap(),
        _backgroundTex(),
        _backgroundVertices(),
        _skyBitmap(),
        _skyTex(),
        _skyVertices(),
        _backgroundCoords(),
        _backgroundNormals(),
        _backgroundTexCoords(),
        _backgroundIndices(),
        _skyCoords(),
        _skyTexCoords(),
        _contourCoords(),
        _shader(),
        _fogShaderSource(),
        _fogShaderFailed(false),
        _a_coord(0),
        _a_normal(0),
        _a_texCoord(0),
        _u_tex(0),
        _u_lightDir(0),
        _u_mvpMat(0),
        _u_fogColor(-1),
        _u_fogHighColor(-1),
        _u_fogSpaceColor(-1),
        _u_fogParams(-1),
        _glResourceManager(),
        _options(options),
        _layers(layers)
    {
    }
    
    BackgroundRenderer::~BackgroundRenderer() {
    }
    
    void BackgroundRenderer::onSurfaceCreated(const std::shared_ptr<GLResourceManager>& resourceManager) {
        _glResourceManager = resourceManager;

        _shader.reset();
        _fogShaderSource.clear();
        _fogShaderFailed = false;
        updateShader();

        _backgroundBitmap.reset();
        _backgroundTex.reset();
        _skyBitmap.reset();
        _skyTex.reset();
    }

    // Rebuilt whenever FogOptions::ShaderSource changes - the same custom applyFog the tile
    // content and the sky get, so one function covers the whole frame.
    bool BackgroundRenderer::updateShader() {
        std::shared_ptr<FogOptions> fogOptions = _options.getFogOptions();
        std::string source = fogOptions ? fogOptions->getShaderSource() : std::string();
        if (_shader && _fogShaderSource == source) {
            return true;
        }
        if (_fogShaderFailed && _fogShaderSource == source) {
            return static_cast<bool>(_shader);
        }

        _fogShaderSource = source;
        _fogShaderFailed = false;

        std::string body = source.empty() ? BACKGROUND_FRAGMENT_SHADER_FOG_BUILTIN : source;
        std::shared_ptr<Shader> shader = _glResourceManager->create<Shader>("background", BACKGROUND_VERTEX_SHADER, BACKGROUND_FRAGMENT_SHADER_PREFIX + body + BACKGROUND_FRAGMENT_SHADER_MAIN);
        if (shader->getProgId() == 0 && !source.empty()) {
            Log::Error("BackgroundRenderer::updateShader: Custom fog shader failed to compile, falling back to the built-in fog");
            _fogShaderFailed = true;
            shader = _glResourceManager->create<Shader>("background", BACKGROUND_VERTEX_SHADER, BACKGROUND_FRAGMENT_SHADER_PREFIX + BACKGROUND_FRAGMENT_SHADER_FOG_BUILTIN + BACKGROUND_FRAGMENT_SHADER_MAIN);
        }
        if (shader->getProgId() == 0) {
            _shader.reset();
            return false;
        }

        _shader = shader;
        GLuint progId = _shader->getProgId();
        _u_tex = _shader->getUniformLoc("u_tex");
        _u_lightDir = _shader->getUniformLoc("u_lightDir");
        _u_mvpMat = _shader->getUniformLoc("u_mvpMat");
        // >= 0 guards, not getUniformLoc: a fog shader that ignores a colour drops that uniform,
        // and getUniformLoc returns 0 for a dropped one - a valid location belonging to something else.
        _u_fogColor = glGetUniformLocation(progId, "uFogColor");
        _u_fogHighColor = glGetUniformLocation(progId, "uFogHighColor");
        _u_fogSpaceColor = glGetUniformLocation(progId, "uFogSpaceColor");
        _u_fogParams = glGetUniformLocation(progId, "uFogParams");
        _a_coord = _shader->getAttribLoc("a_coord");
        _a_normal = _shader->getAttribLoc("a_normal");
        _a_texCoord = _shader->getAttribLoc("a_texCoord");
        return true;
    }

    void BackgroundRenderer::onDrawFrame(const ViewState& viewState, const ResolvedFog& fog, bool drawSkyBand) {
        if (!updateShader()) {
            return;
        }

        std::vector<std::shared_ptr<Layer> > layers = _layers.getAll();

        std::shared_ptr<Bitmap> backgroundBitmap;
        if (!layers.empty() && _options.getBackgroundBitmap() == Options::GetDefaultBackgroundBitmap()) {
            backgroundBitmap = layers.front()->getBackgroundBitmap(viewState);
        }
        if (!backgroundBitmap) {
            backgroundBitmap = _options.getBackgroundBitmap();
        }
        if (_backgroundBitmap != backgroundBitmap) {
            if (backgroundBitmap) {
                _backgroundTex = _glResourceManager->create<Texture>(backgroundBitmap, true, true);
            } else {
                _backgroundTex.reset();
            }
            _backgroundBitmap = backgroundBitmap;
        }

        std::shared_ptr<Bitmap> skyBitmap;
        if (!layers.empty()) {
            skyBitmap = layers.front()->getSkyBitmap(viewState);
        }
        if (!skyBitmap) {
            skyBitmap = _options.getSkyBitmap();
        }
        if (_skyBitmap != skyBitmap) {
            if (skyBitmap) {
                _skyTex = _glResourceManager->create<Texture>(skyBitmap, false, false);
            } else {
                _skyTex.reset();
            }
           _skyBitmap = skyBitmap;
        }

        // Draw sky and background
        if (_skyTex || _backgroundTex) {
            // Prepare for drawing
            glUseProgram(_shader->getProgId());
            // Texture
            glUniform1i(_u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            // Default lighting
            glUniform3f(_u_lightDir, 0, 0, 1);
            // Transformation matrix
            const cglib::mat4x4<float>& mvpMat = viewState.getRTESkyProjectionMat();
            glUniformMatrix4fv(_u_mvpMat, 1, GL_FALSE, mvpMat.data());
            // Coords, texCoords, colors
            glEnableVertexAttribArray(_a_coord);
            glEnableVertexAttribArray(_a_texCoord);
            glVertexAttrib3f(_a_normal, 0, 0, 1);
    
            // Draw background and sky
            if (drawSkyBand && viewState.isSkyVisible()) {
                drawSky(viewState, fog);
            }
            drawBackground(viewState, fog);

            // Disable bound arrays
            glDisableVertexAttribArray(_a_coord);
            glDisableVertexAttribArray(_a_texCoord);
        }
    
        // Draw contour, if in spherical mode. This is a fix for geometry on the back side of the sphere.
        if (_options.getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_SPHERICAL) {
            // Prepare for drawing
            glUseProgram(_shader->getProgId());
            // Texture
            glUniform1i(_u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            // Default lighting
            glUniform3f(_u_lightDir, 0, 0, 1);
            // Transformation matrix
            const cglib::mat4x4<float>& mvpMat = viewState.getRTEModelviewProjectionMat();
            glUniformMatrix4fv(_u_mvpMat, 1, GL_FALSE, mvpMat.data());
            // Coords, texCoords, colors
            glEnableVertexAttribArray(_a_coord);
            glVertexAttrib2f(_a_texCoord, 0, 0);
            glVertexAttrib3f(_a_normal, 0, 0, 1);

            glDepthMask(GL_TRUE);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

            drawContour(viewState);

            glDepthMask(GL_FALSE);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

            glDisableVertexAttribArray(_a_coord);
        }

        GLContext::CheckGLError("BackgroundRenderer::onDrawFrame");
    }
    
    void BackgroundRenderer::onSurfaceDestroyed() {
        _backgroundBitmap.reset();
        _backgroundTex.reset();
        _skyBitmap.reset();
        _skyTex.reset();

        _shader.reset();

        _glResourceManager.reset();
    }
    
    void BackgroundRenderer::setupFogUniforms(const ResolvedFog& fog, bool enabled) {
        // The fog comes from the owner, resolved once for the frame from FogOptions AND the style,
        // so this plane matches the tile content and the sky. It does NOT depend on the terrain:
        // the plane is what fills the view past the loaded tiles at any tilt, so gating it on 3D
        // terrain left a plain 2D map fogging its content and not its background. Passing a zero
        // range disables it, which is what the sky band uses - the sky is the horizon, not ground
        // at an enormous distance, and fogging it would paint the fog colour over the whole sky.
        float rangeStart = 0.0f;
        float invRange = 0.0f;
        float invRangeScale = 1.0f;
        float rangeEnd = 0.0f;
        Color fogColor(0, 0, 0, 0);
        Color highColor(0, 0, 0, 0);
        Color spaceColor(0, 0, 0, 0);
        if (enabled && fog.active()) {
            fogColor = fog.color;
            highColor = fog.highColor;
            spaceColor = fog.spaceColor;
            rangeStart = fog.rangeStart;
            rangeEnd = fog.rangeEnd;
            invRange = 1.0f / std::max(1.0e-9f, fog.rangeEnd - fog.rangeStart);
            invRangeScale = 1.0f / fog.rangeScale;
        }
        if (_u_fogColor >= 0) {
            glUniform4f(_u_fogColor, fogColor.getR() / 255.0f, fogColor.getG() / 255.0f, fogColor.getB() / 255.0f, fogColor.getA() / 255.0f);
        }
        if (_u_fogHighColor >= 0) {
            glUniform4f(_u_fogHighColor, highColor.getR() / 255.0f, highColor.getG() / 255.0f, highColor.getB() / 255.0f, highColor.getA() / 255.0f);
        }
        if (_u_fogSpaceColor >= 0) {
            glUniform4f(_u_fogSpaceColor, spaceColor.getR() / 255.0f, spaceColor.getG() / 255.0f, spaceColor.getB() / 255.0f, spaceColor.getA() / 255.0f);
        }
        if (_u_fogParams >= 0) {
            glUniform4f(_u_fogParams, rangeStart, invRange, invRangeScale, rangeEnd);
        }
    }

    void BackgroundRenderer::drawBackground(const ViewState& viewState, const ResolvedFog& fog) {
        if (!_backgroundTex) {
            return;
        }

        setupFogUniforms(fog, true);

        double intTwoPowZoom = 1 << static_cast<int>(viewState.getZoom());
        const cglib::vec3<double>& focusPos = viewState.getFocusPos();
        const cglib::vec3<double>& cameraPos = viewState.getCameraPos();

        // Texture
        glBindTexture(GL_TEXTURE_2D, _backgroundTex->getTexId());

        // Spherical mode?
        if (_options.getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_SPHERICAL) {
            // Build sphere surface on first call
            if (_backgroundIndices.empty()) {
                BuildSphereSurface(_backgroundCoords, _backgroundNormals, _backgroundTexCoords, _backgroundIndices, SPHERE_TESSELATION_LEVELS_U, SPHERE_TESSELATION_LEVELS_V);
            }

            // Calculate coordinate transformation parameters
            float coordScale = static_cast<float>(Const::WORLD_SIZE / Const::PI);
            float backgroundScale = static_cast<float>(Const::WORLD_SIZE / viewState.getCosHalfFOVXY());
            float scale = static_cast<float>(intTwoPowZoom / Const::WORLD_SIZE);
            double focusPosS = (focusPos(0) != 0 || focusPos(1) != 0 ? std::atan2(focusPos(1), focusPos(0)) / Const::PI + 1 : 0);
            double focusPosT = 0.5 * std::log((1 + cglib::unit(focusPos)(2)) / (1 - cglib::unit(focusPos)(2))) / Const::PI;
            double translateS = -std::floor(focusPosS * scale * backgroundScale);
            double translateT = std::isinf(focusPosT) ? 0 : -std::floor(focusPosT * scale * backgroundScale);

            // Build vertex array
            std::size_t vertexCount = _backgroundCoords.size();
            if (vertexCount != _backgroundVertices.size() * 8) {
                _backgroundVertices.resize(vertexCount * 8);
            }
            for (std::size_t i = 0; i < vertexCount; i++) {
                _backgroundVertices[i * 8 + 0] = static_cast<float>(_backgroundCoords[i](0) * coordScale - cameraPos(0));
                _backgroundVertices[i * 8 + 1] = static_cast<float>(_backgroundCoords[i](1) * coordScale - cameraPos(1));
                _backgroundVertices[i * 8 + 2] = static_cast<float>(_backgroundCoords[i](2) * coordScale - cameraPos(2));

                _backgroundVertices[i * 8 + 3] = _backgroundNormals[i](0);
                _backgroundVertices[i * 8 + 4] = _backgroundNormals[i](1);
                _backgroundVertices[i * 8 + 5] = _backgroundNormals[i](2);

                _backgroundVertices[i * 8 + 6] = static_cast<float>(_backgroundTexCoords[i](0) * scale * backgroundScale + translateS);
                _backgroundVertices[i * 8 + 7] = static_cast<float>(_backgroundTexCoords[i](1) * scale * backgroundScale + translateT);
            }

            // Lighting
            cglib::vec3<float> lightDir = viewState.getFocusPosNormal();
            glUniform3fv(_u_lightDir, 1, lightDir.data());
            
            // Draw
            glEnableVertexAttribArray(_a_normal);
            glVertexAttribPointer(_a_coord, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), _backgroundVertices.data() + 0);
            glVertexAttribPointer(_a_normal, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), _backgroundVertices.data() + 3);
            glVertexAttribPointer(_a_texCoord, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), _backgroundVertices.data() + 6);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_backgroundIndices.size()), GL_UNSIGNED_SHORT, _backgroundIndices.data());
            glDisableVertexAttribArray(_a_normal);
        } else if (_options.getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_PLANAR) {
            // Calculate coordinate transformation parameters
            float backgroundScale = static_cast<float>(viewState.getFar() * 2 / viewState.getCosHalfFOVXY());
            float scale = static_cast<float>(intTwoPowZoom / Const::WORLD_SIZE);
            double translateS = cameraPos(0) * scale;
            double translateT = cameraPos(1) * scale;
            translateS -= std::floor(translateS);
            translateT -= std::floor(translateT);

            // Build vertex array
            std::size_t vertexCount = sizeof(PLANE_COORDS) / sizeof(float) / 3;
            if (_backgroundVertices.size() != vertexCount * 5) {
                _backgroundVertices.resize(vertexCount * 5);
            }
            for (std::size_t i = 0; i < vertexCount; i++) {
                _backgroundVertices[i * 5 + 0] = PLANE_COORDS[i * 3 + 0] * backgroundScale;
                _backgroundVertices[i * 5 + 1] = PLANE_COORDS[i * 3 + 1] * backgroundScale;
                _backgroundVertices[i * 5 + 2] = static_cast<float>(-cameraPos(2));

                _backgroundVertices[i * 5 + 3] = static_cast<float>((PLANE_TEX_COORDS[i * 2 + 0] - 0.5f) * scale * backgroundScale + translateS);
                _backgroundVertices[i * 5 + 4] = static_cast<float>((PLANE_TEX_COORDS[i * 2 + 1] - 0.5f) * scale * backgroundScale + translateT);
            }

            // Draw
            glVertexAttribPointer(_a_coord, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), _backgroundVertices.data() + 0);
            glVertexAttribPointer(_a_texCoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), _backgroundVertices.data() + 3);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(vertexCount));
        }
    }
    
    void BackgroundRenderer::drawSky(const ViewState& viewState, const ResolvedFog& fog) {
        if (!_skyTex) {
            return;
        }

        setupFogUniforms(fog, false);

        // Texture
        glBindTexture(GL_TEXTURE_2D, _skyTex->getTexId());

        // Build sky procedurally
        _skyCoords.clear();
        _skyTexCoords.clear();
        if (_options.getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_SPHERICAL) {
            float coordScale = static_cast<float>(Const::WORLD_SIZE / Const::PI);
            double height0 = SKY_RELATIVE_HEIGHT_SPHERICAL[0] * std::pow(2.0f, -viewState.getZoom() / SKY_HEIGHT_RAMP_SPHERICAL[0]) * coordScale;
            double height1 = SKY_RELATIVE_HEIGHT_SPHERICAL[1] * std::pow(2.0f, -viewState.getZoom() / SKY_HEIGHT_RAMP_SPHERICAL[1]) * coordScale;
            BuildSphereSky(_skyCoords, _skyTexCoords, viewState.getCameraPos(), viewState.getUpVec(), height0, height1, coordScale, SKY_TESSELATION_LEVELS);
        } else if (_options.getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_PLANAR) {
            float coordScale = viewState.getFar() * SKY_SCALE_MULTIPLIER_PLANAR;
            double height0 = SKY_RELATIVE_HEIGHT_PLANAR[0] * std::pow(2.0f, -viewState.getZoom() / SKY_HEIGHT_RAMP_PLANAR[0]) * coordScale;
            double height1 = SKY_RELATIVE_HEIGHT_PLANAR[1] * std::pow(2.0f, -viewState.getZoom() / SKY_HEIGHT_RAMP_PLANAR[1]) * coordScale;
            BuildPlanarSky(_skyCoords, _skyTexCoords, viewState.getCameraPos(), viewState.getFocusPos(), viewState.getUpVec(), height0, height1, coordScale);
        }

        // Copy/pack coordinates
        std::size_t vertexCount = _skyCoords.size();
        if (_skyVertices.size() != vertexCount * 5) {
            _skyVertices.resize(vertexCount * 5);
        }
        for (std::size_t i = 0; i < vertexCount; i++) {
            _skyVertices[i * 5 + 0] = _skyCoords[i](0);
            _skyVertices[i * 5 + 1] = _skyCoords[i](1);
            _skyVertices[i * 5 + 2] = _skyCoords[i](2);

            _skyVertices[i * 5 + 3] = _skyTexCoords[i](0);
            _skyVertices[i * 5 + 4] = _skyTexCoords[i](1);
        }

        // Draw
        glVertexAttribPointer(_a_coord, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), _skyVertices.data() + 0);
        glVertexAttribPointer(_a_texCoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), _skyVertices.data() + 3);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(vertexCount));
    }

    void BackgroundRenderer::drawContour(const ViewState& viewState) {
        _contourCoords.clear();
        if (_options.getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_SPHERICAL) {
            float coordScale = static_cast<float>(Const::WORLD_SIZE / Const::PI);
            BuildSphereContour(_contourCoords, viewState.getCameraPos(), viewState.getUpVec(), coordScale, CONTOUR_TESSELATION_LEVELS);
        }

        // Draw
        std::size_t vertexCount = _contourCoords.size();
        glVertexAttribPointer(_a_coord, 3, GL_FLOAT, GL_FALSE, 0, _contourCoords.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(vertexCount));
    }

    void BackgroundRenderer::BuildPlanarSky(std::vector<cglib::vec3<float> >& coords, std::vector<cglib::vec2<float> >& texCoords, const cglib::vec3<double>& cameraPos, const cglib::vec3<double>& focusPos, const cglib::vec3<double>& upVec, double height0, double height1, float coordScale) {
        cglib::vec3<double> cameraVec = focusPos - cameraPos;

        cglib::vec3<double> axis3 = cglib::unit(cglib::vec3<double>(cameraVec(0), cameraVec(1), 0));
        cglib::vec3<double> axis2 = cglib::unit(upVec);
        cglib::vec3<double> axis1 = cglib::unit(cglib::vector_product(axis3, axis2));

        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 2; i++) {
                double x = (i - 0.5) * coordScale;
                double y = height0 + j * (height1 - height0);
                double z = 0.5 * coordScale;
                coords.emplace_back(cglib::vec3<float>::convert(axis1 * x + axis2 * y + axis3 * z));
                texCoords.emplace_back(0.5f, j);
            }
        }
    }

    void BackgroundRenderer::BuildSphereSky(std::vector<cglib::vec3<float> >& coords, std::vector<cglib::vec2<float> >& texCoords, const cglib::vec3<double>& cameraPos, const cglib::vec3<double>& upVec, double height0, double height1, float coordScale, int tesselate) {
        int vertexCount = (tesselate + 1) * 2;
        coords.reserve(vertexCount);
        texCoords.reserve(vertexCount);

        cglib::vec3<double> axis1a = cglib::unit(cglib::vector_product(cglib::vector_product(cameraPos, upVec), cameraPos));
        cglib::vec3<double> axis2a = cglib::unit(cglib::vector_product(cameraPos, axis1a));

        cglib::vec3<double> axis1b = cglib::unit(upVec);
        cglib::vec3<double> axis2b = cglib::unit(cglib::vector_product(cameraPos, axis1b));

        cglib::vec3<double> origin = cameraPos * (coordScale * coordScale / cglib::norm(cameraPos));
        double radius = std::sqrt(std::max(0.0, coordScale * coordScale - cglib::norm(origin)));

        for (int i = 0; i <= tesselate; i++) {
            double u = 2.0 * Const::PI * (static_cast<double>(i < tesselate ? i : 0) / tesselate - 0.5);
            double x = std::cos(u);
            double y = std::sin(u);

            coords.emplace_back(cglib::vec3<float>::convert((axis1a * x + axis2a * y) * radius + (axis1b * x + axis2b * y) * height0 + origin - cameraPos));
            coords.emplace_back(cglib::vec3<float>::convert((axis1a * x + axis2a * y) * radius + (axis1b * x + axis2b * y) * height1 + origin - cameraPos));
            texCoords.emplace_back(0.5f, 0.0f);
            texCoords.emplace_back(0.5f, 1.0f);
        }
    }

    void BackgroundRenderer::BuildSphereContour(std::vector<cglib::vec3<float> >& coords, const cglib::vec3<double>& cameraPos, const cglib::vec3<double>& upVec, float coordScale, int tesselate) {
        int vertexCount = (tesselate + 1) * 2;
        coords.reserve(vertexCount);

        cglib::vec3<double> axis1a = cglib::unit(cglib::vector_product(cglib::vector_product(cameraPos, upVec), cameraPos));
        cglib::vec3<double> axis2a = cglib::unit(cglib::vector_product(cameraPos, axis1a));

        cglib::vec3<double> origin = cameraPos * (coordScale * coordScale / cglib::norm(cameraPos));
        double radius = std::sqrt(std::max(0.0, coordScale * coordScale - cglib::norm(origin)));

        for (int i = 0; i <= tesselate; i++) {
            double u = 2.0 * Const::PI * (static_cast<double>(i < tesselate ? i : 0) / tesselate - 0.5);
            double x = std::cos(u);
            double y = std::sin(u);

            coords.emplace_back(cglib::vec3<float>::convert(origin - cameraPos));
            coords.emplace_back(cglib::vec3<float>::convert((axis1a * x + axis2a * y) * radius + origin - cameraPos));
        }
    }
    
    void BackgroundRenderer::BuildSphereSurface(std::vector<cglib::vec3<double> >& coords, std::vector<cglib::vec3<float> >& normals, std::vector<cglib::vec2<float> >& texCoords, std::vector<unsigned short>& indices, int tesselateU, int tesselateV) {
        int vertexCount = (tesselateU + 1) * (tesselateV + 1);
        int indexCount = 6 * tesselateU * tesselateV;
        coords.reserve(vertexCount);
        normals.reserve(vertexCount);
        texCoords.reserve(vertexCount);
        indices.reserve(indexCount);

        // Note: we use simple longitude/latitude tesselation scheme. Recursive tetrahedra-based surface would contain fewer vertexCount but produces texture artifacts near poles
        for (int j = 0; j <= tesselateV; j++) {
            double v = Const::PI * (static_cast<double>(j) / tesselateV - 0.5);
            float t = 0.5f * std::log((1.0f + std::sin(v)) / (1.0f - std::sin(v))) / static_cast<float>(Const::PI);
            for (int i = 0; i <= tesselateU; i++) {
                double u = 2.0 * Const::PI * (static_cast<double>(i < tesselateU ? i : 0) / tesselateU - 0.5);
                float s = 2.0f * i / tesselateU;

                double x = std::cos(u) * std::cos(v);
                double y = std::sin(u) * std::cos(v);
                double z = std::sin(v);

                coords.emplace_back(x, y, z);
                normals.emplace_back(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                texCoords.emplace_back(s, t);
            }
        }

        for (int j = 0; j < tesselateV; j++) {
            for (int i = 0; i < tesselateU; i++) {
                int i00 = (i + 0) + (j + 0) * (tesselateU + 1);
                int i01 = (i + 0) + (j + 1) * (tesselateU + 1);
                int i10 = (i + 1) + (j + 0) * (tesselateU + 1);
                int i11 = (i + 1) + (j + 1) * (tesselateU + 1);

                indices.push_back(static_cast<unsigned short>(i00));
                indices.push_back(static_cast<unsigned short>(i10));
                indices.push_back(static_cast<unsigned short>(i01));
                indices.push_back(static_cast<unsigned short>(i10));
                indices.push_back(static_cast<unsigned short>(i11));
                indices.push_back(static_cast<unsigned short>(i01));
            }
        }
    }

    const float BackgroundRenderer::PLANE_COORDS[] = {
        -0.5f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, 0.5f, 0.0f, 0.5f, -0.5f, 0.0f
    };

    const float BackgroundRenderer::PLANE_TEX_COORDS[] = {
        0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f
    };

    const float BackgroundRenderer::SKY_SCALE_MULTIPLIER_PLANAR = 2.0f / std::sqrt(3.0f);
    const float BackgroundRenderer::SKY_RELATIVE_HEIGHT_PLANAR[] = { -0.02f, 0.06f };
    const float BackgroundRenderer::SKY_HEIGHT_RAMP_PLANAR[] = { 40.0f, -18.0f };
    const float BackgroundRenderer::SKY_RELATIVE_HEIGHT_SPHERICAL[] = { -0.05f, 0.1f };
    const float BackgroundRenderer::SKY_HEIGHT_RAMP_SPHERICAL[2] = { 3.0f, 4.0f };

    const std::string BackgroundRenderer::BACKGROUND_VERTEX_SHADER = R"GLSL(
        #version 100
        attribute vec3 a_coord;
        attribute vec3 a_normal;
        attribute vec2 a_texCoord;
        uniform vec3 u_lightDir;
        uniform mat4 u_mvpMat;
        varying vec4 v_color;
        varying vec2 v_texCoord;
        void main() {
            float lighting = max(0.0, dot(a_normal, u_lightDir)) * 0.5 + 0.5;
            v_color = vec4(lighting, lighting, lighting, 1.0);
            v_texCoord = a_texCoord;
            gl_Position = u_mvpMat * vec4(a_coord, 1.0);
        }
    )GLSL";

    const std::string BackgroundRenderer::BACKGROUND_FRAGMENT_SHADER_PREFIX = R"GLSL(
        #version 100
        precision mediump float;
        uniform sampler2D u_tex;
        uniform lowp vec4 uFogColor;      // rgb = fog colour, a = how opaque the fog gets at full distance
        uniform lowp vec4 uFogHighColor;
        uniform lowp vec4 uFogSpaceColor;
        uniform highp vec4 uFogParams;    // range start, 1 / (end - start), internal -> range units, range end
        varying lowp vec4 v_color;
        #ifdef GL_FRAGMENT_PRECISION_HIGH
        varying highp vec2 v_texCoord;
        #else
        varying mediump vec2 v_texCoord;
        #endif
    )GLSL";

    const std::string BackgroundRenderer::BACKGROUND_FRAGMENT_SHADER_FOG_BUILTIN = R"GLSL(
        vec4 applyFog(vec4 color, float amount, float dist) {
            return vec4(mix(color.rgb, uFogColor.rgb, amount), color.a);
        }
    )GLSL";

    const std::string BackgroundRenderer::BACKGROUND_FRAGMENT_SHADER_MAIN = R"GLSL(
        void main() {
            vec4 color = texture2D(u_tex, v_texCoord) * v_color;
            if (color.a == 0.0) {
                discard;
            }
            // Same fog as the tile content (vt applyFog): eye distance is 1/gl_FragCoord.w, in
            // range units. The background plane is what fills the view past the loaded tiles and
            // past the terrain view distance, so it has to fade into the fog as well - otherwise
            // the ground ends on a hard band of background colour instead of reaching the sky.
            highp float dist = uFogParams.z / max(1.0e-9, gl_FragCoord.w);
            lowp float amount = clamp((dist - uFogParams.x) * uFogParams.y, 0.0, 1.0) * uFogColor.a;
            gl_FragColor = applyFog(color, amount, dist);
        }
    )GLSL";
}
