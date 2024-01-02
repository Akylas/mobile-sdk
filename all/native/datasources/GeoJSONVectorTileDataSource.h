/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_GEOJSONVECTORTILEDATASOURCE_H_
#define _CARTO_GEOJSONVECTORTILEDATASOURCE_H_

#include "core/Variant.h"
#include "datasources/TileDataSource.h"

#include <memory>
#include <mutex>
#include <string>

namespace carto {
    namespace mbvtbuilder {
        class MBVTTileBuilder;
    }

    class Projection;
    class FeatureCollection;
    
    /**
     * A tile data source that builds vector tiles from GeoJSON inputs.
     */
    class GeoJSONVectorTileDataSource : public TileDataSource {
    public:
        /**
         * Constructs a GeoJSONVectorTileDataSource object.
         * Note: EPSG3857 projection is used.
         * @param minZoom The minimum zoom level supported by this data source.
         * @param maxZoom The maximum zoom level supported by this data source.
         */
        GeoJSONVectorTileDataSource(int minZoom, int maxZoom);
        virtual ~GeoJSONVectorTileDataSource();

        /**
         * Returns the simplification tolerance in tile pixels.
         * @return The simplification tolerance in tile pixels.
         */
        float getSimplifyTolerance() const;
        /**
         * Sets the simplification tolerance in tile pixels.
         * @param tolerance The simplification tolerance in tile pixels. The default is 1.0. Use 0.0 to disable simplification.
         */
        void setSimplifyTolerance(float tolerance);

        /**
         * Returns the default layer buffer in tile pixels.
         * @return The default layer buffer in tile pixels.
         */
        float getDefaultLayerBuffer() const;
        /**
         * Sets the default layer buffer in tile pixels. This buffer value will be used for all layers created afterwards.
         * @param buffer The default layer buffer in tile pixels. The default is 4.0. Use 0.0 to disable any buffering. Use larger values (up to 32.0) in case of clipping artifacts.
         */
        void setDefaultLayerBuffer(float tolerance);

        /**
         * Creates a new layer with specified name.
         * @param name The name for the layer.
         * @return The layer index of the created layer.
         * @throws std::runtime_error If an error occured during creation of the layer.
         */
        int createLayer(const std::string& name);

        /**
         * Sets the features of the specified layer.
         * @param layerIndex The index of the layer. A layer with empty name will be created if it does not exist yet.
         * @param geoJSON A geojson type variant that MUST contain single FeatureColletion element.
         * @throws std::runtime_error If an error occured during updating the layer.
         */
        void setLayerGeoJSON(int layerIndex, const Variant& geoJSON);

        /**
         * Sets the features of the specified layer.
         * @param layerIndex The index of the layer. A layer with empty name will be created if it does not exist yet.
         * @param geoJSON A geojson type variant that MUST contain single Feature element.
         * @throws std::runtime_error If an error occured during updating the layer.
         */
        void addGeoJSONFeature(int layerIndex, const Variant& geoJSON);

        /**
         * Sets the features of the specified layer if it already exists.
         * @param layerIndex The index of the layer. A layer with empty name will be created if it does not exist yet.
         * @param geoJSON A geojson type variant that MUST contain single Feature element.
         * @throws std::runtime_error If an error occured during updating the layer.
         */
        void updateGeoJSONFeature(int layerIndex, const Variant& geoJSON);

        /**
         * Sets the features of the specified layer.
         * @param layerIndex The index of the layer. A layer with empty name will be created if it does not exist yet.
         * @param geoJSON A geojson string that MUST contain single FeatureColletion element.
         * @throws std::runtime_error If an error occured during updating the layer.
         */
        void setLayerGeoJSONString(int layerIndex, const std::string& geoJSON);

        /**
         * Sets the features of the specified layer.
         * @param layerIndex The index of the layer. A layer with empty name will be created if it does not exist yet.
         * @param geoJSON A geojson string that MUST contain single Feature element.
         * @throws std::runtime_error If an error occured during updating the layer.
         */
        void addGeoJSONStringFeature(int layerIndex, const std::string& geoJSON);

        /**
         * Sets the features of the specified layer if it already exists.
         * @param layerIndex The index of the layer. A layer with empty name will be created if it does not exist yet.
         * @param geoJSON A geojson string that MUST contain single Feature element.
         * @throws std::runtime_error If an error occured during updating the layer.
         */
        void updateGeoJSONStringFeature(int layerIndex, const std::string& geoJSON);

        /**
         * Sets the features of the specified layer.
         * @param layerIndex The index of the layer. A layer with empty name will be created if it does not exist yet.
         * @param id id of the feature to remove.
         * @throws std::runtime_error If an error occured during updating the layer.
         */
        void removeGeoJSONFeature(int layerIndex, const Variant& id);

        /**
         * Sets the feature collection of the specified layer.
         * @param layerIndex The index of the layer. A layer with empty name will be created if it does not exist yet.
         * @param projection Projection for the features in featureCollection. Can be null if the coordinates are based on WGS84.
         * @param featureCollection The feature collection for the specified layer.
         * @throws std::runtime_error If an error occured during updating the layer.
         */
        void setLayerFeatureCollection(int layerIndex, const std::shared_ptr<Projection>& projection, const std::shared_ptr<FeatureCollection>& featureCollection);

        /**
         * Deletes an existing layer.
         * @param layerIndex The index of layer to delete.
         */
        void deleteLayer(int layerIndex);
        
        virtual MapBounds getDataExtent() const;

        virtual std::shared_ptr<TileData> loadTile(const MapTile& mapTile);
    
    private:
        std::unique_ptr<mbvtbuilder::MBVTTileBuilder> _tileBuilder;
        mutable std::mutex _mutex;
    };
    
}

#endif
