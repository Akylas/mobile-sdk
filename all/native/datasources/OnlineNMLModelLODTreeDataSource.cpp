#ifdef _CARTO_NMLMODELLODTREE_SUPPORT

#include "OnlineNMLModelLODTreeDataSource.h"
#include "core/BinaryData.h"
#include "graphics/ViewState.h"
#include "projections/EPSG3857.h"
#include "renderers/components/CullState.h"
#include "utils/Log.h"
#include "utils/NetworkUtils.h"

#include <boost/lexical_cast.hpp>

#include <stdext/miniz.h>

#include <nml/Package.h>

namespace carto {

    OnlineNMLModelLODTreeDataSource::OnlineNMLModelLODTreeDataSource(const std::string& serviceURL) :
        NMLModelLODTreeDataSource(std::make_shared<EPSG3857>()),
        _serviceURL(serviceURL)
    {
    }
    
    OnlineNMLModelLODTreeDataSource::~OnlineNMLModelLODTreeDataSource() {
    }
    
    std::vector<NMLModelLODTreeDataSource::MapTile> OnlineNMLModelLODTreeDataSource::loadMapTiles(const std::shared_ptr<CullState>& cullState) {
        MapBounds bounds;
        bounds.expandToContain(_projection->fromInternal(cullState->getEnvelope().getBounds().getMin()));
        bounds.expandToContain(_projection->fromInternal(cullState->getEnvelope().getBounds().getMax()));
    
        std::map<std::string, std::string> urlParams;
        urlParams["q"] = "MapTiles";
        urlParams["mapbounds_x0"] = boost::lexical_cast<std::string>(bounds.getMin().getX());
        urlParams["mapbounds_y0"] = boost::lexical_cast<std::string>(bounds.getMin().getY());
        urlParams["mapbounds_x1"] = boost::lexical_cast<std::string>(bounds.getMax().getX());
        urlParams["mapbounds_y1"] = boost::lexical_cast<std::string>(bounds.getMax().getY());
        urlParams["width"] = boost::lexical_cast<std::string>(_projection->getBounds().getDelta().getX());
        std::string url = NetworkUtils::BuildURLFromParameters(_serviceURL, urlParams);
    
        Log::Debugf("OnlineNMLModelLODTreeDataSource: Request %s", url.c_str());
        std::shared_ptr<BinaryData> response;
        if (!NetworkUtils::GetHTTP(url, response, Log::IsShowDebug())) {
            Log::Error("OnlineNMLModelLODTreeDataSource: Failed to receive tile list.");
            return std::vector<MapTile>();
        }
    
        DataInputStream gzipStream(*response->getDataPtr());
        int gzipDataSize = gzipStream.readInt();
        std::vector<unsigned char> gzipData = gzipStream.readBytes(gzipDataSize);
        std::vector<unsigned char> data;
        if (!miniz::inflate_gzip(gzipData.data(), gzipData.size(), data)) {
            Log::Error("OnlineNMLModelLODTreeDataSource: Failed to decompress tile list data.");
            return std::vector<MapTile>();
        }
    
        DataInputStream dataStream(data);
        
        std::vector<MapTile> mapTiles;
        
        while (true) {
            long long mapTileId = dataStream.readLongLong();
            if (mapTileId == -1) {
                break;
            }
            long long modelLODTreeId = dataStream.readLongLong();
            double mapPosX = dataStream.readDouble();
            double mapPosY = dataStream.readDouble();
            double mapPosZ = dataStream.readDouble();
    
            MapTile mapTile(mapTileId, MapPos(mapPosX, mapPosY, mapPosZ), modelLODTreeId);
            mapTiles.push_back(mapTile);
        }
        return mapTiles;
    }
    
    std::shared_ptr<NMLModelLODTree> OnlineNMLModelLODTreeDataSource::loadModelLODTree(const MapTile& mapTile) {
        std::map<std::string, std::string> urlParams;
        urlParams["q"] = "ModelLODTree";
        urlParams["id"] = boost::lexical_cast<std::string>(mapTile.modelLODTreeId);
        std::string url = NetworkUtils::BuildURLFromParameters(_serviceURL, urlParams);
    
        Log::Debugf("OnlineNMLModelLODTreeDataSource: Request %s", url.c_str());
        std::shared_ptr<BinaryData> response;
        if (!NetworkUtils::GetHTTP(url, response, Log::IsShowDebug())) {
            Log::Error("OnlineNMLModelLODTreeDataSource: Failed to receive LOD tree.");
            return std::shared_ptr<NMLModelLODTree>();
        }
    
        DataInputStream gzipStream(*response->getDataPtr());
        int gzipDataSize = gzipStream.readInt();
        std::vector<unsigned char> gzipData = gzipStream.readBytes(gzipDataSize);
        std::vector<unsigned char> data;
        if (!miniz::inflate_gzip(gzipData.data(), gzipData.size(), data)) {
            Log::Error("OnlineNMLModelLODTreeDataSource: Failed to decompress LOD tree.");
            return std::shared_ptr<NMLModelLODTree>();
        }
    
        DataInputStream dataStream(data);
        int nmlModelLODTreeSize = dataStream.readInt();
        std::vector<unsigned char> nmlModelLODTreeData = dataStream.readBytes(nmlModelLODTreeSize);
        std::shared_ptr<nml::ModelLODTree> sourceModelLODTree = std::make_shared<nml::ModelLODTree>(protobuf::message(nmlModelLODTreeData.size() > 0 ? &nmlModelLODTreeData[0] : nullptr, nmlModelLODTreeSize));
    
        // Model info, proxies
        NMLModelLODTree::ProxyMap proxyMap;
        while (true) {
            int modelId = dataStream.readInt();
            if (modelId == -1) {
                break;
            }
            std::string metaDataEnc = dataStream.readString();
            std::multimap<std::string, std::string> metaDataMulti = NetworkUtils::URLDecodeMap(metaDataEnc);
            double mapPosX = dataStream.readDouble();
            double mapPosY = dataStream.readDouble();
            double mapPosZ = dataStream.readDouble();

            MapPos mapPos(mapPosX, mapPosY, mapPosZ);
            std::map<std::string, std::string> metaData(metaDataMulti.begin(), metaDataMulti.end());
            proxyMap.emplace(modelId, NMLModelLODTree::Proxy(modelId, mapPos, metaData));
        }
    
        // Mesh bindings
        NMLModelLODTree::MeshBindingsMap meshBindingsMap;
        while (true) {
            int nodeId = dataStream.readInt();
            if (nodeId == -1) {
                break;
            }
            std::string localId = dataStream.readString();
            long long meshId = dataStream.readLongLong();
            if (meshId == -1) {
                meshId = dataStream.readLongLong();
                int nmlMeshOpSize = dataStream.readInt();
                std::vector<unsigned char> nmlMeshOpData = dataStream.readBytes(nmlMeshOpSize);
                std::shared_ptr<nml::MeshOp> meshOp = std::make_shared<nml::MeshOp>(protobuf::message(nmlMeshOpData.size() > 0 ? &nmlMeshOpData[0] : nullptr, nmlMeshOpSize));
                meshBindingsMap[nodeId].push_back(NMLModelLODTree::MeshBinding(meshId, localId, meshOp));
            } else {
                meshBindingsMap[nodeId].push_back(NMLModelLODTree::MeshBinding(meshId, localId));
            }
        }
    
        // Texture bindings
        NMLModelLODTree::TextureBindingsMap textureBindingsMap;
        while (true) {
            int nodeId = dataStream.readInt();
            if (nodeId == -1) {
                break;
            }
            std::string localId = dataStream.readString();
            long long textureId = dataStream.readLongLong();
            int level = dataStream.readInt();
    
            textureBindingsMap[nodeId].push_back(NMLModelLODTree::TextureBinding(textureId, level, localId));
        }
    
        std::shared_ptr<NMLModelLODTree> modelLODTree = std::make_shared<NMLModelLODTree>(mapTile.modelLODTreeId, mapTile.mapPos, _projection, sourceModelLODTree, proxyMap, meshBindingsMap, textureBindingsMap);
        return modelLODTree;
    }
    
    std::shared_ptr<nml::Mesh> OnlineNMLModelLODTreeDataSource::loadMesh(long long meshId) {
        std::map<std::string, std::string> urlParams;
        urlParams["q"] = "Meshes";
        urlParams["ids"] = boost::lexical_cast<std::string>(meshId);
        std::string url = NetworkUtils::BuildURLFromParameters(_serviceURL, urlParams);
    
        Log::Debugf("OnlineNMLModelLODTreeDataSource: Request %s", url.c_str());
        std::shared_ptr<BinaryData> response;
        if (!NetworkUtils::GetHTTP(url, response, Log::IsShowDebug())) {
            Log::Error("OnlineNMLModelLODTreeDataSource: Failed to receive mesh data.");
            return std::shared_ptr<nml::Mesh>();
        }
    
        DataInputStream gzipStream(*response->getDataPtr());
        gzipStream.readLongLong();
        int gzipDataSize = gzipStream.readInt();
        std::vector<unsigned char> gzipData = gzipStream.readBytes(gzipDataSize);
        std::vector<unsigned char> data;
        if (!miniz::inflate_gzip(gzipData.data(), gzipData.size(), data)) {
            Log::Error("OnlineNMLModelLODTreeDataSource: Failed to decompress mesh data.");
            return std::shared_ptr<nml::Mesh>();
        }
    
        std::shared_ptr<nml::Mesh> mesh = std::make_shared<nml::Mesh>(protobuf::message(data.size() > 0 ? &data[0] : nullptr, data.size()));
        return mesh;
    }
    
    std::shared_ptr<nml::Texture> OnlineNMLModelLODTreeDataSource::loadTexture(long long textureId, int level) {
        std::map<std::string, std::string> urlParams;
        urlParams["q"] = "Textures";
        urlParams["ids"] = boost::lexical_cast<std::string>(textureId);
        std::string url = NetworkUtils::BuildURLFromParameters(_serviceURL, urlParams);
    
        Log::Debugf("OnlineNMLModelLODTreeDataSource: Request %s", url.c_str());
        std::shared_ptr<BinaryData> response;
        if (!NetworkUtils::GetHTTP(url, response, Log::IsShowDebug())) {
            Log::Error("OnlineNMLModelLODTreeDataSource: Failed to receive texture data.");
            return std::shared_ptr<nml::Texture>();
        }
    
        DataInputStream gzipStream(*response->getDataPtr());
        gzipStream.readLongLong();
        int gzipDataSize = gzipStream.readInt();
        std::vector<unsigned char> gzipData = gzipStream.readBytes(gzipDataSize);
        std::vector<unsigned char> data;
        if (!miniz::inflate_gzip(gzipData.data(), gzipData.size(), data)) {
            Log::Error("OnlineNMLModelLODTreeDataSource: Failed to decompress texture data.");
            return std::shared_ptr<nml::Texture>();
        }
    
        std::shared_ptr<nml::Texture> texture = std::make_shared<nml::Texture>(protobuf::message(data.size() > 0 ? &data[0] : nullptr, data.size()));
        return texture;
    }
    
    OnlineNMLModelLODTreeDataSource::DataInputStream::DataInputStream(const std::vector<unsigned char>& data) : _data(data), _offset(0)
    {
    }
    
    unsigned char OnlineNMLModelLODTreeDataSource::DataInputStream::readByte() {
        if (_offset >= _data.size()) {
            Log::Error("OnlineNMLModelLODTreeDataSource::DataInputStream: reading past the end");
            return 0;
        }
        return _data[_offset++];
    }
    
    int OnlineNMLModelLODTreeDataSource::DataInputStream::readInt() {
        int value = 0;
        for (int i = 0; i < 4; i++) {
            value = (value << 8) | readByte();
        }
        return value;
    }
    
    long long OnlineNMLModelLODTreeDataSource::DataInputStream::readLongLong() {
        long long value = 0;
        for (int i = 0; i < 8; i++) {
            value = (value << 8) | readByte();
        }
        return value;
    }
    
    float OnlineNMLModelLODTreeDataSource::DataInputStream::readFloat() {
        int value = readInt();
        return *reinterpret_cast<float*>(&value);
    }
    
    double OnlineNMLModelLODTreeDataSource::DataInputStream::readDouble() {
        long long value = readLongLong();
        return *reinterpret_cast<double*>(&value);
    }
    
    std::string OnlineNMLModelLODTreeDataSource::DataInputStream::readString() {
        unsigned int length = readByte();
        length = (length << 8) | readByte();
        if (_offset + length > _data.size()) {
            Log::Error("OnlineNMLModelLODTreeDataSource::DataInputStream: reading past the end");
            return std::string();
        }
        std::size_t offset = _offset;
        _offset += length;
        return std::string(reinterpret_cast<const char*>(_data.data() + offset), reinterpret_cast<const char*>(_data.data() + _offset));
    }
    
    std::vector<unsigned char> OnlineNMLModelLODTreeDataSource::DataInputStream::readBytes(std::size_t size) {
        if (_offset + size > _data.size()) {
            Log::Error("OnlineNMLModelLODTreeDataSource::DataInputStream: reading past the end");
            return std::vector<unsigned char>();
        }
        std::size_t offset = _offset;
        _offset += size;
        return std::vector<unsigned char>(_data.begin() + offset, _data.begin() + _offset);
    }
    
}

#endif
