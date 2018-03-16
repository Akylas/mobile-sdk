#include "TextureManager.h"
#include "graphics/Texture.h"
#include "utils/Log.h"

namespace carto {

    TextureManager::TextureManager() :
        _glThreadId(),
        _createQueue(),
        _deleteTexIdQueue(),
        _mutex()
    {
    }
    
    TextureManager::~TextureManager() {
    }

    std::thread::id TextureManager::getGLThreadId() const {
        std::lock_guard<std::mutex> lock(_mutex);

        return _glThreadId;
    }
    
    void TextureManager::setGLThreadId(std::thread::id id) {
        std::lock_guard<std::mutex> lock(_mutex);

        _glThreadId = id;
    }
    
    std::shared_ptr<Texture> TextureManager::createTexture(const std::shared_ptr<Bitmap>& bitmap, bool genMipmaps, bool repeat) {
        std::lock_guard<std::mutex> lock(_mutex);

        std::shared_ptr<Texture> texture(
            new Texture(shared_from_this(), bitmap, genMipmaps, repeat), [this](Texture* texture) {
                deleteTexture(texture);
            }
        );

        _createQueue.push_back(texture);
        return texture;
    }

    void TextureManager::processTextures() {
        std::vector<std::weak_ptr<Texture> > createQueue;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (std::this_thread::get_id() != _glThreadId) {
                Log::Warn("TextureManager::processTextures: Method called from wrong thread!");
                return;
            }

            if (!_deleteTexIdQueue.empty()) {
                glDeleteTextures(static_cast<unsigned int>(_deleteTexIdQueue.size()), _deleteTexIdQueue.data());
                _deleteTexIdQueue.clear();
            }

            for (const std::weak_ptr<Texture>& textureWeak : _createQueue) {
                if (auto texture = textureWeak.lock()) {
                    texture->load();
                }
            }
            std::swap(createQueue, _createQueue); // release the textures only after lock is released
        }

        GLContext::CheckGLError("TextureManager::processTextures");
    }

    void TextureManager::deleteTexture(Texture* texture) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (texture) {
            if (std::this_thread::get_id() == _glThreadId) {
                texture->unload();
            }
            else {
                if (texture->_texId != 0) {
                    _deleteTexIdQueue.push_back(texture->_texId);
                }
            }
            delete texture;
        }
    }

}
