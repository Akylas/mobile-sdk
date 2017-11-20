#include "NMLModelRenderer.h"
#include "components/ThreadWorker.h"
#include "datasources/VectorDataSource.h"
#include "graphics/Shader.h"
#include "graphics/ShaderManager.h"
#include "graphics/ViewState.h"
#include "graphics/utils/GLContext.h"
#include "layers/VectorLayer.h"
#include "projections/Projection.h"
#include "renderers/MapRenderer.h"
#include "renderers/components/RayIntersectedElement.h"
#include "utils/Log.h"

#include <nml/GLModel.h>
#include <nml/GLResourceManager.h>

namespace {

    struct GLResourceDeleter : carto::ThreadWorker {
        GLResourceDeleter(std::shared_ptr<carto::nml::GLResourceManager> glResourceManager) : _glResourceManager(std::move(glResourceManager)) { }

        virtual void operator () () {
            _glResourceManager->deleteAll();
        }

    private:
        std::shared_ptr<carto::nml::GLResourceManager> _glResourceManager;
    };

}

namespace carto {

    NMLModelRenderer::NMLModelRenderer() :
        _glResourceManager(),
        _glModelMap(),
        _elements(),
        _tempElements(),
        _mapRenderer(),
        _options(),
        _mutex()
    {
    }
    
    NMLModelRenderer::~NMLModelRenderer() {
        if (_glResourceManager) {
            if (auto mapRenderer = _mapRenderer.lock()) {
                mapRenderer->addRenderThreadCallback(std::make_shared<GLResourceDeleter>(_glResourceManager));
            }
        }
    }
        
    void NMLModelRenderer::addElement(const std::shared_ptr<NMLModel>& element) {
        _tempElements.push_back(element);
    }
    
    void NMLModelRenderer::refreshElements() {
        std::lock_guard<std::mutex> lock(_mutex);
        _elements.clear();
        _elements.swap(_tempElements);
    }
        
    void NMLModelRenderer::updateElement(const std::shared_ptr<NMLModel>& element) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (std::find(_elements.begin(), _elements.end(), element) == _elements.end()) {
            _elements.push_back(element);
        }
    }
    
    void NMLModelRenderer::removeElement(const std::shared_ptr<NMLModel>& element) {
        std::lock_guard<std::mutex> lock(_mutex);
        _elements.erase(std::remove(_elements.begin(), _elements.end(), element), _elements.end());
    }
    
    void NMLModelRenderer::setMapRenderer(const std::weak_ptr<MapRenderer>& mapRenderer) {
        std::lock_guard<std::mutex> lock(_mutex);
        _mapRenderer = mapRenderer;
    }

    void NMLModelRenderer::setOptions(const std::weak_ptr<Options>& options) {
        std::lock_guard<std::mutex> lock(_mutex);
        _options = options;
    }

    void NMLModelRenderer::offsetLayerHorizontally(double offset) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        for (const std::shared_ptr<NMLModel>& element : _elements) {
            element->getDrawData()->offsetHorizontally(offset);
        }
    }

    void NMLModelRenderer::onSurfaceCreated(const std::shared_ptr<ShaderManager>& shaderManager, const std::shared_ptr<TextureManager>& textureManager) {
        _glResourceManager = std::make_shared<nml::GLResourceManager>();
        _glModelMap.clear();
    }

    bool NMLModelRenderer::onDrawFrame(float deltaSeconds, const ViewState& viewState) {
        std::lock_guard<std::mutex> lock(_mutex);

        std::shared_ptr<Options> options = _options.lock();
        if (!options) {
            return false;
        }
    
        // Set expected GL state
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);

        // Calculate lighting state
        Color ambientColor = options->getAmbientLightColor();
        cglib::vec4<float> ambientLightColor = cglib::vec4<float>(ambientColor.getR(), ambientColor.getG(), ambientColor.getB(), ambientColor.getA()) * (1.0f / 255.0f);
        Color mainColor = options->getMainLightColor();
        cglib::vec4<float> mainLightColor = cglib::vec4<float>(mainColor.getR(), mainColor.getG(), mainColor.getB(), mainColor.getA()) * (1.0f / 255.0f);
        MapVec mainDir = options->getMainLightDirection();
        cglib::vec3<float> mainLightDir = cglib::unit(cglib::vec3<float>::convert(cglib::transform_vector(cglib::vec3<double>(mainDir.getX(), mainDir.getY(), mainDir.getZ()), viewState.getModelviewMat())));

        // Draw models
        cglib::mat4x4<float> projMat = cglib::mat4x4<float>::convert(viewState.getProjectionMat());
        for (const std::shared_ptr<NMLModel>& element : _elements) {
            const NMLModelDrawData& drawData = *element->getDrawData();
            std::shared_ptr<nml::Model> sourceModel = drawData.getSourceModel();
            std::shared_ptr<nml::GLModel> glModel = _glModelMap[sourceModel];
            if (!glModel) {
                glModel = std::make_shared<nml::GLModel>(*sourceModel);
                glModel->create(*_glResourceManager);
                _glModelMap[sourceModel] = glModel;
            }
    
            cglib::mat4x4<float> mvMat = cglib::mat4x4<float>::convert(viewState.getModelviewMat() * drawData.getLocalMat());
            nml::RenderState renderState(projMat, mvMat, ambientLightColor, mainLightColor, -mainLightDir);

            glModel->draw(*_glResourceManager, renderState);
        }

        // Remove stale models
        for (auto it = _glModelMap.begin(); it != _glModelMap.end(); ) {
            if (it->first.expired()) {
                it = _glModelMap.erase(it);
            } else {
                it++;
            }
        }

        // Dispose unused models
        _glResourceManager->deleteUnused();

        // Restore expected GL state
        glDepthMask(GL_TRUE);
        glDisable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);

        GLContext::CheckGLError("NMLModelRenderer::onDrawFrame()");
        return false;
    }

    void NMLModelRenderer::onSurfaceDestroyed() {
        _glResourceManager.reset();
        _glModelMap.clear();
    }

    void NMLModelRenderer::calculateRayIntersectedElements(const std::shared_ptr<VectorLayer>& layer, const cglib::ray3<double>& ray, const ViewState& viewState, std::vector<RayIntersectedElement>& results) const {
        std::lock_guard<std::mutex> lock(_mutex);
        
        for (const std::shared_ptr<NMLModel>& element : _elements) {
            const NMLModelDrawData& drawData = *element->getDrawData();
            
            std::shared_ptr<nml::Model> sourceModel = drawData.getSourceModel();
            auto modelIt = _glModelMap.find(sourceModel);
            if (modelIt == _glModelMap.end()) {
                continue;
            }
            std::shared_ptr<nml::GLModel> glModel = modelIt->second;
    
            cglib::mat4x4<double> modelMat = drawData.getLocalMat();
            cglib::mat4x4<double> invModelMat = cglib::inverse(modelMat);
    
            cglib::ray3<double> rayModel = cglib::transform_ray(ray, invModelMat);
            cglib::bbox3<double> modelBounds = cglib::bbox3<double>::convert(glModel->getBounds());
            if (!cglib::intersect_bbox(modelBounds, rayModel)) {
                continue;
            }

            std::vector<nml::RayIntersection> intersections;
            glModel->calculateRayIntersections(rayModel, intersections);
            
            for (std::size_t i = 0; i < intersections.size(); i++) {
                cglib::vec3<double> pos = cglib::transform_point(intersections[i].pos, modelMat);
                MapPos clickPos(pos(0), pos(1), pos(2));
                MapPos projectedClickPos = layer->getDataSource()->getProjection()->fromInternal(clickPos);
                int priority = static_cast<int>(results.size());
                results.push_back(RayIntersectedElement(std::static_pointer_cast<VectorElement>(element), layer, projectedClickPos, projectedClickPos, priority, true));
            }
        }
    }
    
}
