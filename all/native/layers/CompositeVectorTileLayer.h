/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_COMPOSITEVECTORTILELAYER_H_
#define _CARTO_COMPOSITEVECTORTILELAYER_H_

#include "layers/VectorTileLayer.h"

#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace carto {
    class TileDataSource;
    class VectorTileDecoder;
    class ElevationDecoder;
    namespace mvt { struct ResolvedLayerConfig; }

    namespace CompositeSourceType {
        /**
         * The kind of an external data source added to a CompositeVectorTileLayer.
         */
        enum CompositeSourceType {
            /**
             * A raster tile source, drawn as a RasterTileLayer at its style slot.
             */
            COMPOSITE_SOURCE_TYPE_RASTER,
            /**
             * An RGB-encoded elevation source, drawn as a HillshadeRasterTileLayer at its style slot.
             */
            COMPOSITE_SOURCE_TYPE_HILLSHADE,
            /**
             * Another MBVT/protobuf source (including ContourTileDataSource), drawn at its style
             * slot as its own child VectorTileLayer using the master decoder, filtered to its own
             * layer name. Kept separate (not merged) so it overzooms independently via its own
             * MaxOverzoomLevel and does not need the DEM at the target zoom.
             */
            COMPOSITE_SOURCE_TYPE_VECTOR
        };
    }

    /**
     * A VectorTileLayer that can weave named external data sources (raster, hillshade,
     * merged vector / contour) into the master CartoCSS style's layer order.
     *
     * Each external source is placed at the position of a matching layer name in the style
     * project's "layers" array, and configured by a matching '#name { ... }' block in the
     * CartoCSS (e.g. raster-opacity, hillshade-exaggeration), including zoom- and nuti-
     * parameter-dependent expressions. Raster and hillshade sources are rendered as their
     * own draped child layers interleaved between the master style layers; merged vector
     * sources are folded into the master source and styled normally.
     *
     * Sources can be added and removed at runtime.
     */
    class CompositeVectorTileLayer : public VectorTileLayer {
    public:
        /**
         * Constructs a CompositeVectorTileLayer from a master vector data source and decoder.
         * @param dataSource The master vector tile data source.
         * @param decoder The tile decoder (must be an MBVectorTileDecoder for external source
         *                configuration and placement to work).
         */
        CompositeVectorTileLayer(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<VectorTileDecoder>& decoder);
        virtual ~CompositeVectorTileLayer();

        /**
         * Adds a named external data source. For raster and hillshade types the source is
         * drawn as its own child layer at the style slot named 'name'. For the vector type
         * the source is merged into the master source (see addVectorDataSource).
         * @param name The source name; must match a layer name in the style "layers" array.
         * @param dataSource The external data source.
         * @param type The source type.
         * @param elevationDecoder Optional elevation decoder for hillshade sources. If null,
         *        it is resolved from the data source 'encoding' metadata ("terrarium"/"mapbox").
         */
        void addExternalDataSource(const std::string& name, const std::shared_ptr<TileDataSource>& dataSource, CompositeSourceType::CompositeSourceType type, const std::shared_ptr<ElevationDecoder>& elevationDecoder = std::shared_ptr<ElevationDecoder>());
        /**
         * Adds a named MBVT/protobuf source (including ContourTileDataSource) merged into the
         * master source and styled by the master CartoCSS. Equivalent to addExternalDataSource
         * with COMPOSITE_SOURCE_TYPE_VECTOR.
         * @param name The source name; its layers must be declared in the master style.
         * @param dataSource The vector data source to merge.
         */
        void addVectorDataSource(const std::string& name, const std::shared_ptr<TileDataSource>& dataSource);
        /**
         * Removes the named external data source (of any type). Returns true if removed.
         * @param name The source name.
         * @return True if a source was removed.
         */
        bool removeExternalDataSource(const std::string& name);
        /**
         * Returns the names of all registered external data sources.
         * @return The registered external source names.
         */
        std::vector<std::string> getExternalDataSourceNames() const;

        /**
         * Returns whether single-pass segmented rendering is enabled (Milestone 6, optional).
         * @return True if single-pass rendering is enabled. The default is false.
         */
        bool isSinglePassRenderingEnabled() const;
        /**
         * Sets whether to use the optional single-pass segmented renderer instead of the
         * default one-vt-pass-per-segment path. Intended for A/B comparison; currently a
         * no-op placeholder until the single-pass renderer lands.
         * @param enabled True to enable single-pass rendering.
         */
        void setSinglePassRenderingEnabled(bool enabled);

    protected:
        virtual void setComponents(const std::shared_ptr<CancelableThreadPool>& envelopeThreadPool,
                                   const std::shared_ptr<CancelableThreadPool>& tileThreadPool,
                                   const std::weak_ptr<Options>& options,
                                   const std::weak_ptr<MapRenderer>& mapRenderer,
                                   const std::weak_ptr<TouchHandler>& touchHandler);

        virtual void loadData(const std::shared_ptr<CullState>& cullState);
        virtual void offsetLayerHorizontally(double offset);
        virtual bool isUpdateInProgress() const;
        virtual void calculateRayIntersectedElements(const cglib::ray3<double>& ray, const ViewState& viewState, std::vector<RayIntersectedElement>& results) const;

        virtual bool onDrawFrame(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState);
        virtual bool onDrawFrame3D(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState);

    private:
        struct ExternalSource {
            std::string name;
            CompositeSourceType::CompositeSourceType type;
            std::shared_ptr<TileDataSource> dataSource;
            std::shared_ptr<Layer> childLayer; // raster/hillshade child; null for merged vector
        };

        // One ordered draw step after the layer's own group-0 render: either an external
        // raster/hillshade child, or an internal VectorTileLayer rendering a later style-layer
        // group with a fixed rendererLayerFilter (the filter is applied at tile-build time, so
        // each group needs its own stable-filtered layer - a single renderer cannot be
        // re-filtered per frame).
        enum DrawItemKind { DRAW_ITEM_EXTERNAL, DRAW_ITEM_VT_GROUP };
        struct DrawItem {
            DrawItemKind kind;
            std::string slot;                  // external source name (DRAW_ITEM_EXTERNAL)
            std::shared_ptr<Layer> groupLayer; // internal group layer (DRAW_ITEM_VT_GROUP); held as
                                               // Layer so protected virtuals are reachable via friend
        };

        static std::shared_ptr<ElevationDecoder> resolveElevationDecoder(const std::shared_ptr<TileDataSource>& dataSource);
        static std::string buildFilterString(const std::vector<std::string>& group);

        void wireChild(const std::shared_ptr<Layer>& child);
        void unwireChild(const std::shared_ptr<Layer>& child);
        std::shared_ptr<Layer> makeGroupLayer(const std::string& filter);
        void rebuildDrawItems();
        void applyExternalChildZoomRange(const ExternalSource& source);
        const ExternalSource* findExternalSource(const std::string& name) const;
        void applyConfig(const ExternalSource& source, const mvt::ResolvedLayerConfig& config, const ViewState& viewState);
        // Applies '#name' config symbolizer values to merged vector sources whose generation
        // parameters live on the data source (currently ContourTileDataSource). Called off the
        // render thread (from loadData); only re-applies changed values to avoid reload loops.
        void applyVectorSourceConfigs();
        bool renderComposite(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState, bool terrain);

        std::vector<ExternalSource> _externalSources;
        std::vector<DrawItem> _drawItems;
        std::map<std::string, std::map<std::string, float> > _lastVectorConfig; // per-source applied contour params
        bool _singlePassRenderingEnabled;

        // Cached component handles for wiring child layers added after setComponents().
        bool _componentsSet;
        std::weak_ptr<Options> _childOptions;
        std::weak_ptr<MapRenderer> _childMapRenderer;
        std::weak_ptr<TouchHandler> _childTouchHandler;

        mutable std::recursive_mutex _sourceMutex;
    };

}

#endif
