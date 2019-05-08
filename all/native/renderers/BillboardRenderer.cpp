#include "BillboardRenderer.h"
#include "graphics/Shader.h"
#include "graphics/ShaderManager.h"
#include "graphics/Texture.h"
#include "graphics/TextureManager.h"
#include "graphics/ViewState.h"
#include "graphics/utils/GLContext.h"
#include "layers/VectorLayer.h"
#include "projections/ProjectionSurface.h"
#include "renderers/drawdatas/BillboardDrawData.h"
#include "renderers/components/RayIntersectedElement.h"
#include "renderers/components/StyleTextureCache.h"
#include "renderers/components/BillboardSorter.h"
#include "styles/AnimationStyle.h"
#include "styles/BillboardStyle.h"
#include "utils/Const.h"
#include "utils/Log.h"
#include "vectorelements/Billboard.h"

#include <cglib/mat.h>

namespace carto {

    void BillboardRenderer::CalculateBillboardAxis(const BillboardDrawData& drawData, const ViewState& viewState, cglib::vec3<float>& xAxis, cglib::vec3<float>& yAxis) {
        const ViewState::RotationState& rotationState = viewState.getRotationState();
        switch (drawData.getOrientationMode()) {
        case BillboardOrientation::BILLBOARD_ORIENTATION_GROUND:
            xAxis = drawData.getXAxis();
            yAxis = drawData.getYAxis();
            break;
        case BillboardOrientation::BILLBOARD_ORIENTATION_FACE_CAMERA_GROUND:
            xAxis = cglib::unit(cglib::vector_product(rotationState.yAxis, drawData.getZAxis()));
            yAxis = cglib::unit(cglib::vector_product(drawData.getZAxis(), xAxis));
            break;
        case BillboardOrientation::BILLBOARD_ORIENTATION_FACE_CAMERA:
        default:
            xAxis = rotationState.xAxis;
            yAxis = rotationState.yAxis;
            break;
        }
    }
    
    bool BillboardRenderer::CalculateBillboardCoords(const BillboardDrawData& drawData, const ViewState& viewState,
                                                     std::vector<float>& coordBuf, std::size_t drawDataIndex, float sizeScale)
    {
        cglib::vec3<float> translate = cglib::vec3<float>::convert(drawData.getPos() - viewState.getCameraPos());
        if (cglib::dot_product(drawData.getZAxis(), translate) > 0) {
            return false;
        }
        
        const std::array<cglib::vec2<float>, 4>& coords = drawData.getCoords();
        for (int i = 0; i < 4; i++) {
            std::size_t coordIndex = (drawDataIndex * 4 + i) * 3;
            float x = coords[i](0);
            float y = coords[i](1);
            
            float scale = drawData.isScaleWithDPI() ? viewState.getUnitToDPCoef() : viewState.getUnitToPXCoef();
            scale *= sizeScale;

            // Calculate scaling
            switch (drawData.getScalingMode()) {
            case BillboardScaling::BILLBOARD_SCALING_WORLD_SIZE:
                break;
            case BillboardScaling::BILLBOARD_SCALING_SCREEN_SIZE:
                x *= scale;
                y *= scale;
                break;
            case BillboardScaling::BILLBOARD_SCALING_CONST_SCREEN_SIZE:
            default:
                float coef = static_cast<float>(scale * drawData.getCameraPlaneZoomDistance());
                x *= coef;
                y *= coef;
                break;
            }
            
            // Calculate axis
            cglib::vec3<float> xAxis, yAxis;
            CalculateBillboardAxis(drawData, viewState, xAxis, yAxis);
        
            // Build coordinates
            coordBuf[coordIndex + 0] = x * xAxis(0) + y * yAxis(0) + translate(0);
            coordBuf[coordIndex + 1] = x * xAxis(1) + y * yAxis(1) + translate(1);
            coordBuf[coordIndex + 2] = x * xAxis(2) + y * yAxis(2) + translate(2);
        }
        return true;
    }
    
    BillboardRenderer::BillboardRenderer() :
        _layer(),
        _elements(),
        _tempElements(),
        _colorBuf(),
        _coordBuf(),
        _indexBuf(),
        _texCoordBuf(),
        _shader(),
        _a_color(0),
        _a_coord(0),
        _a_texCoord(0),
        _u_mvpMat(0),
        _u_tex(0),
        _mutex()
    {
    }
    
    BillboardRenderer::~BillboardRenderer() {
    }
    
    void BillboardRenderer::offsetLayerHorizontally(double offset) {
        // Offset current draw data batch horizontally by the required amount
        std::lock_guard<std::recursive_mutex> lock(_mutex);
    
        for (const std::shared_ptr<Billboard>& element : _elements) {
            element->getDrawData()->offsetHorizontally(offset);
        }
    }
    
    void BillboardRenderer::onSurfaceCreated(const std::shared_ptr<ShaderManager>& shaderManager, const std::shared_ptr<TextureManager>& textureManager) {
        static ShaderSource shaderSource("billboard", &BILLBOARD_VERTEX_SHADER, &BILLBOARD_FRAGMENT_SHADER);

        _shader = shaderManager->createShader(shaderSource);

        // Get shader variables locations
        glUseProgram(_shader->getProgId());
        _a_color = _shader->getAttribLoc("a_color");
        _a_coord = _shader->getAttribLoc("a_coord");
        _a_texCoord = _shader->getAttribLoc("a_texCoord");
        _u_mvpMat = _shader->getUniformLoc("u_mvpMat");
        _u_tex = _shader->getUniformLoc("u_tex");

        // Drop elements
        std::vector<std::shared_ptr<Billboard>> elements;
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            std::swap(elements, _elements);
        }
        for (const std::shared_ptr<Billboard>& element : elements) {
            element->setDrawData(std::shared_ptr<BillboardDrawData>());
        }
    }
    
    bool BillboardRenderer::onDrawFrame(float deltaSeconds, BillboardSorter& billboardSorter, StyleTextureCache& styleCache, const ViewState& viewState) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
    
        // Billboards can't be rendered in layer order, they have to be sorted globally and drawn from back to front
        bool refresh = false;
        for (auto it = _elements.begin(); it != _elements.end(); ) {
            std::shared_ptr<Billboard> element = *it++;
            std::shared_ptr<BillboardDrawData> drawData = element->getDrawData();

            // Update animation state
            bool phaseOut = drawData->getRenderer().lock() != shared_from_this() || (drawData->isHideIfOverlapped() && drawData->isOverlapping());
            if (auto animStyle = drawData->getAnimationStyle()) {
                float transition = drawData->getTransition();
                float step = (phaseOut ? -1.0f : 1.0f);
                float duration = (phaseOut ? animStyle->getPhaseOutDuration() : animStyle->getPhaseInDuration()) / animStyle->getRelativeSpeed();
                if ((transition < 1.0f && step > 0.0f) || (transition > 0.0f && step < 0.0f)) {
                    if (duration > 0.0f) {
                        drawData->setTransition(transition + step * (transition == 0.0f || transition == 1.0f ? 0.01f : deltaSeconds) / duration);
                        refresh = true;
                    } else {
                        drawData->setTransition(phaseOut ? 0.0f : 1.0f);
                    }
                }
            } else {
                drawData->setTransition(phaseOut ? 0.0f : 1.0f);
            }

            // If the element has been removed and has become invisible, remove it from the list
            if (drawData->getRenderer().lock() != shared_from_this() && drawData->getTransition() == 0.0f) {
                it = _elements.erase(--it);
                continue;
            }

            // Add the draw data to the sorter
            if (calculateBaseBillboardDrawData(drawData, viewState)) {
                billboardSorter.add(drawData);
            }
        }
        return refresh;
    }
    
    void BillboardRenderer::onDrawFrameSorted(float deltaSeconds,
                                              const std::vector<std::shared_ptr<BillboardDrawData> >& billboardDrawDatas,
                                              StyleTextureCache& styleCache, const ViewState& viewState)
    {
        // Get layer opacity
        float opacity = 1.0f;
        if (std::shared_ptr<VectorLayer> layer = getLayer()) {
            opacity = layer->getOpacity();
        }
    
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        // Prepare for drawing
        glUseProgram(_shader->getProgId());
        // Coords, texCoords, colors
        glEnableVertexAttribArray(_a_coord);
        glEnableVertexAttribArray(_a_texCoord);
        glEnableVertexAttribArray(_a_color);
        //Matrix
        const cglib::mat4x4<float>& mvpMat = viewState.getRTEModelviewProjectionMat();
        glUniformMatrix4fv(_u_mvpMat, 1, GL_FALSE, mvpMat.data());
        // Texture
        glUniform1i(_u_tex, 0);
        
        // Draw billboards, batch by bitmap
        _drawDataBuffer.clear();
        std::shared_ptr<Bitmap> prevBitmap;
        for (const std::shared_ptr<BillboardDrawData>& drawData : billboardDrawDatas) {
            if (std::shared_ptr<Bitmap> bitmap = drawData->getBitmap()) {
                if (prevBitmap && prevBitmap != bitmap) {
                    drawBatch(opacity, styleCache, viewState);
                    _drawDataBuffer.clear();
                }
        
                _drawDataBuffer.push_back(std::move(drawData));
                prevBitmap = bitmap;
            }
        }
    
        if (prevBitmap) {
            drawBatch(opacity, styleCache, viewState);
        }
    
        glDisableVertexAttribArray(_a_coord);
        glDisableVertexAttribArray(_a_texCoord);
        glDisableVertexAttribArray(_a_color);
    
        GLContext::CheckGLError("BillboardRenderer::onDrawFrameSorted");
    }
    
    void BillboardRenderer::onSurfaceDestroyed() {
        _shader.reset();
    }
    
    std::size_t BillboardRenderer::getElementCount() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _elements.size();
    }
        
    void BillboardRenderer::addElement(const std::shared_ptr<Billboard>& element) {
        if (std::shared_ptr<BillboardDrawData> drawData = element->getDrawData()) {
            drawData->setRenderer(shared_from_this());
            _tempElements.push_back(element);
        }
    }
    
    void BillboardRenderer::refreshElements() {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _elements.clear();
        _elements.swap(_tempElements);
    }
    
    void BillboardRenderer::updateElement(const std::shared_ptr<Billboard>& element) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (std::find(_elements.begin(), _elements.end(), element) == _elements.end()) {
            if (std::shared_ptr<BillboardDrawData> drawData = element->getDrawData()) {
                drawData->setRenderer(shared_from_this());
                _elements.push_back(element);
            }
        }
    }
    
    void BillboardRenderer::removeElement(const std::shared_ptr<Billboard>& element) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (std::shared_ptr<BillboardDrawData> drawData = element->getDrawData()) {
            drawData->setRenderer(std::weak_ptr<BillboardRenderer>());
            if (!drawData->getAnimationStyle()) {
                drawData->setTransition(0.0f);
                _elements.erase(std::remove(_elements.begin(), _elements.end(), element), _elements.end());
            }
        } else {
            _elements.erase(std::remove(_elements.begin(), _elements.end(), element), _elements.end());
        }
    }
    
    void BillboardRenderer::setLayer(const std::shared_ptr<VectorLayer>& layer) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _layer = layer;
    }
    
    std::shared_ptr<VectorLayer> BillboardRenderer::getLayer() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _layer.lock();
    }
    
    void BillboardRenderer::calculateRayIntersectedElements(const std::shared_ptr<VectorLayer>& layer, const cglib::ray3<double>& ray, const ViewState& viewState, std::vector<RayIntersectedElement>& results) const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        
        // Buffer for world coordinates
        std::vector<float> coordBuf(12);
        for (const std::shared_ptr<Billboard>& element : _elements) {
            std::shared_ptr<BillboardDrawData> drawData = element->getDrawData();

            // Don't detect clicks on overlapping billboards that are hidden or removed elements
            if (drawData->getRenderer().lock() != shared_from_this()) {
                continue;
            }
            if (drawData->getTransition() == 0) {
                continue;
            }
    
            if (!CalculateBillboardCoords(*drawData, viewState, coordBuf, 0, drawData->getClickScale())) {
                continue;
            }
            cglib::vec3<double> originShift = viewState.getCameraPos();
            cglib::vec3<double> topLeft = cglib::vec3<double>(coordBuf[0], coordBuf[1], coordBuf[2]) + originShift;
            cglib::vec3<double> bottomLeft = cglib::vec3<double>(coordBuf[3], coordBuf[4], coordBuf[5]) + originShift;
            cglib::vec3<double> topRight = cglib::vec3<double>(coordBuf[6], coordBuf[7], coordBuf[8]) + originShift;
            cglib::vec3<double> bottomRight = cglib::vec3<double>(coordBuf[9], coordBuf[10], coordBuf[11]) + originShift;
            
            // If either triangle intersects the ray, add the element to result list
            double t = 0;
            if (cglib::intersect_triangle(topLeft, bottomLeft, topRight, ray, &t) ||
                cglib::intersect_triangle(bottomLeft, bottomRight, topRight, ray, &t))
            {
                results.push_back(RayIntersectedElement(std::static_pointer_cast<VectorElement>(element), layer, ray(t), drawData->getPos(), true));
            }
        }
    }
        
    void BillboardRenderer::BuildAndDrawBuffers(GLuint a_color,
                                                GLuint a_coord,
                                                GLuint a_texCoord,
                                                std::vector<unsigned char>& colorBuf,
                                                std::vector<float>& coordBuf,
                                                std::vector<unsigned short>& indexBuf,
                                                std::vector<float>& texCoordBuf,
                                                std::vector<std::shared_ptr<BillboardDrawData> >& drawDataBuffer,
                                                const cglib::vec2<float>& texCoordScale,
                                                float opacity,
                                                StyleTextureCache& styleCache,
                                                const ViewState& viewState)
    {
        // Resize the buffers, if necessary
        if (coordBuf.size() < drawDataBuffer.size() * 4 * 3) {
            coordBuf.resize(std::min(drawDataBuffer.size() * 4 * 3, GLContext::MAX_VERTEXBUFFER_SIZE * 3));
            texCoordBuf.resize(std::min(drawDataBuffer.size() * 4 * 2, GLContext::MAX_VERTEXBUFFER_SIZE * 2));
            colorBuf.resize(std::min(drawDataBuffer.size() * 4 * 4, GLContext::MAX_VERTEXBUFFER_SIZE * 4));
            indexBuf.resize(std::min(drawDataBuffer.size() * 6, GLContext::MAX_VERTEXBUFFER_SIZE));
        }
        
        // Calculate and draw buffers
        std::size_t drawDataIndex = 0;
        for (std::size_t i = 0; i < drawDataBuffer.size(); i++) {
            const std::shared_ptr<BillboardDrawData>& drawData = drawDataBuffer[i];

            // Alpha value
            int alpha = std::min(256, static_cast<int>(256 * opacity * AnimationStyle::CalculateTransition(drawData->getAnimationStyle() ? drawData->getAnimationStyle()->getFadeAnimationType() : AnimationType::ANIMATION_TYPE_NONE, drawData->getTransition())));
            
            // Check for possible overflow in the buffers
            if ((drawDataIndex + 1) * 6 > GLContext::MAX_VERTEXBUFFER_SIZE) {
                // If it doesn't fit, stop and draw the buffers
                glVertexAttribPointer(a_coord, 3, GL_FLOAT, GL_FALSE, 0, coordBuf.data());
                glVertexAttribPointer(a_texCoord, 2, GL_FLOAT, GL_FALSE, 0, texCoordBuf.data());
                glVertexAttribPointer(a_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, colorBuf.data());
                glDrawElements(GL_TRIANGLES, drawDataIndex * 6, GL_UNSIGNED_SHORT, indexBuf.data());
                // Start filling buffers from the beginning
                drawDataIndex = 0;
            }
            
            // If invisible, skip further steps
            if (drawData->getTransition() == 0) {
                continue;
            }
            
            // Calculate coordinates
            float relativeSize = AnimationStyle::CalculateTransition(drawData->getAnimationStyle() ? drawData->getAnimationStyle()->getSizeAnimationType() : AnimationType::ANIMATION_TYPE_NONE, drawData->getTransition());
            if (!CalculateBillboardCoords(*drawData, viewState, coordBuf, drawDataIndex, relativeSize)) {
                continue;
            }
            
            // Billboards with ground orientation (like some texts) have to be flipped to readable
            bool flip = false;
            if (drawData->isFlippable() && drawData->getOrientationMode() == BillboardOrientation::BILLBOARD_ORIENTATION_GROUND) {
                float dAngle = std::fmod(viewState.getRotation() - drawData->getRotation() + 360.0f, 360.0f);
                flip = dAngle > 90 && dAngle < 270;
            }
            
            // Calculate texture coordinates
            std::size_t texCoordIndex = drawDataIndex * 4 * 2;
            if (!flip) {
                texCoordBuf[texCoordIndex + 0] = 0.0f;
                texCoordBuf[texCoordIndex + 1] = texCoordScale(1);
                texCoordBuf[texCoordIndex + 2] = 0.0f;
                texCoordBuf[texCoordIndex + 3] = 0.0f;
                texCoordBuf[texCoordIndex + 4] = texCoordScale(0);
                texCoordBuf[texCoordIndex + 5] = texCoordScale(1);
                texCoordBuf[texCoordIndex + 6] = texCoordScale(0);
                texCoordBuf[texCoordIndex + 7] = 0.0f;
            } else {
                texCoordBuf[texCoordIndex + 0] = texCoordScale(0);
                texCoordBuf[texCoordIndex + 1] = 0.0f;
                texCoordBuf[texCoordIndex + 2] = texCoordScale(0);
                texCoordBuf[texCoordIndex + 3] = texCoordScale(1);
                texCoordBuf[texCoordIndex + 4] = 0.0f;
                texCoordBuf[texCoordIndex + 5] = 0.0f;
                texCoordBuf[texCoordIndex + 6] = 0.0f;
                texCoordBuf[texCoordIndex + 7] = texCoordScale(1);
            }
            
            // Calculate colors
            const Color& color = drawData->getColor();
            std::size_t colorIndex = drawDataIndex * 4 * 4;
            for (int i = 0; i < 16; i += 4) {
                colorBuf[colorIndex + i + 0] = static_cast<unsigned char>((color.getR() * alpha) >> 8);
                colorBuf[colorIndex + i + 1] = static_cast<unsigned char>((color.getG() * alpha) >> 8);
                colorBuf[colorIndex + i + 2] = static_cast<unsigned char>((color.getB() * alpha) >> 8);
                colorBuf[colorIndex + i + 3] = static_cast<unsigned char>((color.getA() * alpha) >> 8);
            }
            
            // Calculate indices
            std::size_t indexIndex = drawDataIndex * 6;
            unsigned short vertexIndex = static_cast<unsigned short>(drawDataIndex * 4);
            indexBuf[indexIndex + 0] = vertexIndex + 0;
            indexBuf[indexIndex + 1] = vertexIndex + 1;
            indexBuf[indexIndex + 2] = vertexIndex + 2;
            indexBuf[indexIndex + 3] = vertexIndex + 1;
            indexBuf[indexIndex + 4] = vertexIndex + 3;
            indexBuf[indexIndex + 5] = vertexIndex + 2;
            
            drawDataIndex++;
        }
        
        glVertexAttribPointer(a_coord, 3, GL_FLOAT, GL_FALSE, 0, coordBuf.data());
        glVertexAttribPointer(a_texCoord, 2, GL_FLOAT, GL_FALSE, 0, texCoordBuf.data());
        glVertexAttribPointer(a_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, colorBuf.data());
        glDrawElements(GL_TRIANGLES, drawDataIndex * 6, GL_UNSIGNED_SHORT, indexBuf.data());
    }
        
    bool BillboardRenderer::calculateBaseBillboardDrawData(const std::shared_ptr<BillboardDrawData>& drawData, const ViewState& viewState) {
        std::shared_ptr<Billboard> baseBillboard = drawData->getBaseBillboard().lock();
        if (!baseBillboard) {
            return true;
        }
        
        const std::shared_ptr<BillboardDrawData>& baseBillboardDrawData = baseBillboard->getDrawData();
        if (!baseBillboardDrawData) {
            return false;
        }
        
        if (!calculateBaseBillboardDrawData(baseBillboardDrawData, viewState)) {
            return false;
        }
        
        // Billboard is attached to another billboard, calculate position before sorting
        cglib::vec3<double> baseBillboardPos = baseBillboardDrawData->getPos();
        cglib::vec3<float> baseBillboardTranslate = cglib::vec3<float>::convert(baseBillboardPos - viewState.getCameraPos());
        if (cglib::dot_product(baseBillboardDrawData->getZAxis(), baseBillboardTranslate) > 0) {
            return false;
        }

        float halfSize = baseBillboardDrawData->getSize() * 0.5f;
        cglib::vec2<float> labelAnchorVec(((drawData->getAttachAnchorPointX() - baseBillboardDrawData->getAnchorPointX()) * halfSize),
                                          ((drawData->getAttachAnchorPointY() - baseBillboardDrawData->getAnchorPointY()) / baseBillboardDrawData->getAspect() * halfSize));
        
        if (baseBillboardDrawData->getRotation() != 0) {
            labelAnchorVec = cglib::transform(labelAnchorVec, cglib::rotate2_matrix(static_cast<float>(baseBillboardDrawData->getRotation() * Const::DEG_TO_RAD)));
        }
        
        const ViewState::RotationState& rotationState = viewState.getRotationState();
        
        float scale = baseBillboardDrawData->isScaleWithDPI() ? viewState.getUnitToDPCoef() : viewState.getUnitToPXCoef();
        // Calculate scaling
        switch (baseBillboardDrawData->getScalingMode()) {
        case BillboardScaling::BILLBOARD_SCALING_WORLD_SIZE:
            break;
        case BillboardScaling::BILLBOARD_SCALING_SCREEN_SIZE:
            labelAnchorVec *= scale;
            break;
        case BillboardScaling::BILLBOARD_SCALING_CONST_SCREEN_SIZE:
        default:
            {
                const cglib::mat4x4<double>& mvpMat = viewState.getModelviewProjectionMat();
                double distance = baseBillboardPos(0) * mvpMat(3, 0) + baseBillboardPos(1) * mvpMat(3, 1) + baseBillboardPos(2) * mvpMat(3, 2) + mvpMat(3, 3);
                float coef = static_cast<float>(distance * viewState.get2PowZoom() / viewState.getZoom0Distance() * scale);
                labelAnchorVec *= coef;
            }
            break;
        }
        
        // Calculate orientation
        cglib::vec3<float> xAxis, yAxis;
        CalculateBillboardAxis(*baseBillboardDrawData, viewState, xAxis, yAxis);

        // Calculate delta, update position
        cglib::vec3<double> delta = cglib::vec3<double>::convert(xAxis * labelAnchorVec(0) + yAxis * labelAnchorVec(1));
        if (std::shared_ptr<ProjectionSurface> projectionSurface = viewState.getProjectionSurface()) {
            drawData->setPos(baseBillboardPos + delta, *projectionSurface);
        }
        return true;
    }
        
    void BillboardRenderer::drawBatch(float opacity, StyleTextureCache& styleCache, const ViewState& viewState) {
        // Bind texture
        const std::shared_ptr<Bitmap>& bitmap = _drawDataBuffer.front()->getBitmap();
        std::shared_ptr<Texture> texture = styleCache.get(bitmap);
        if (!texture) {
            texture = styleCache.create(bitmap, _drawDataBuffer.front()->isGenMipmaps(), false);
        }
        glBindTexture(GL_TEXTURE_2D, texture->getTexId());
        
        // Draw the draw datas, multiple passes may be necessary
        BuildAndDrawBuffers(_a_color, _a_coord, _a_texCoord, _colorBuf, _coordBuf, _indexBuf, _texCoordBuf, _drawDataBuffer,
                            texture->getTexCoordScale(), opacity, styleCache, viewState);
    }
    
    const std::string BillboardRenderer::BILLBOARD_VERTEX_SHADER = R"GLSL(
        #version 100
        attribute vec4 a_coord;
        attribute vec2 a_texCoord;
        attribute vec4 a_color;
        varying vec2 v_texCoord;
        varying vec4 v_color;
        uniform mat4 u_mvpMat;
        void main() {
            v_texCoord = a_texCoord;
            v_color = a_color;
            gl_Position = u_mvpMat * a_coord;
        }
    )GLSL";

    const std::string BillboardRenderer::BILLBOARD_FRAGMENT_SHADER = R"GLSL(
        #version 100
        precision mediump float;
        varying mediump vec2 v_texCoord;
        varying lowp vec4 v_color;
        uniform sampler2D u_tex;
        void main() {
            vec4 color = texture2D(u_tex, v_texCoord) * v_color;
            if (color.a == 0.0) {
                discard;
            }
            gl_FragColor = color;
        }
    )GLSL";

}
