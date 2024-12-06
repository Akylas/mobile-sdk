package com.akylas.cartotest.ui.main;

import android.Manifest;
import android.animation.Animator;
import android.annotation.SuppressLint;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.provider.Settings;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.SeekBar;
import android.widget.TextView;

import com.akylas.cartotest.R;
import com.carto.components.Options;
import com.carto.components.PanningMode;
import com.carto.core.BinaryData;
import com.carto.core.MapBounds;
import com.carto.core.MapPos;
import com.carto.core.MapPosVector;
import com.carto.core.MapPosVectorVector;
import com.carto.core.MapRange;
import com.carto.core.MapVec;
import com.carto.core.ScreenBounds;
import com.carto.core.StringMap;
import com.carto.core.StringVector;
import com.carto.core.Variant;
import com.carto.datasources.GeoJSONVectorTileDataSource;
import com.carto.datasources.HTTPTileDataSource;
import com.carto.datasources.MergedMBVTTileDataSource;
import com.carto.datasources.MultiTileDataSource;
import com.carto.datasources.LocalVectorDataSource;
import com.carto.datasources.MBTilesTileDataSource;
import com.carto.datasources.PersistentCacheTileDataSource;
import com.carto.datasources.TileDataSource;
import com.carto.geometry.Feature;
import com.carto.geometry.FeatureCollection;
import com.carto.geometry.GeoJSONGeometryReader;
import com.carto.geometry.GeoJSONGeometryWriter;
import com.carto.geometry.Geometry;
import com.carto.geometry.LineGeometry;
import com.carto.geometry.MultiLineGeometry;
import com.carto.geometry.PointGeometry;
import com.carto.geometry.PolygonGeometry;
import com.carto.geometry.VectorTileFeatureCollection;
import com.carto.graphics.Color;
import com.carto.layers.HillshadeRasterTileLayer;
import com.carto.layers.RasterTileFilterMode;
import com.carto.layers.RasterTileLayer;
import com.carto.layers.TileSubstitutionPolicy;
import com.carto.layers.VectorLayer;
import com.carto.layers.VectorTileLayer;
import com.carto.projections.EPSG4326;
import com.carto.projections.Projection;
import com.carto.rastertiles.MapBoxElevationDataDecoder;
import com.carto.routing.RouteMatchingRequest;
import com.carto.routing.RouteMatchingResult;
import com.carto.routing.RoutingRequest;
import com.carto.routing.RoutingResult;
import com.carto.routing.MultiValhallaOfflineRoutingService;
import com.carto.search.SearchRequest;
import com.carto.search.VectorTileSearchService;
import com.carto.styles.CartoCSSStyleSet;
import com.carto.styles.CompiledStyleSet;
import com.carto.styles.LineJoinType;
import com.carto.styles.LineStyleBuilder;
import com.carto.ui.MapClickInfo;
import com.carto.ui.MapEventListener;
import com.carto.ui.MapView;
import com.carto.utils.AssetUtils;
import com.carto.utils.ZippedAssetPackage;
import com.carto.vectorelements.Line;
import com.carto.vectortiles.MBVectorTileDecoder;

import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Paths;
import java.util.Timer;
import java.util.TimerTask;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.appcompat.widget.AppCompatSeekBar;
import androidx.fragment.app.Fragment;

public class SecondFragment extends Fragment {
    private final String TAG = "SecondFragment";


//    class MathRouteTask extends TimerTask
//    {
//        MathRouteTask(MultiValhallaOfflineRoutingService routingService, Projection projection, String profile) {
//            super();
//            this.profile = profile;
//            this.projection = projection;
//            this.routingService = routingService;
//        }
//        MultiValhallaOfflineRoutingService routingService;
//        Projection projection;
//        String profile;
//        public void run()
//        {
//            matchRouteTest(this.routingService, this.projection, this.profile);
//        }
//    }

    public static SecondFragment newInstance() {
        return new SecondFragment();
    }

    private static final int REQUEST_PERMISSIONS_CODE_WRITE_STORAGE = 1435;
    private static final int REQUEST_PERMISSIONS_MANAGE_STORAGE = 1436;
    MapView mapView;
    MapBoxElevationDataDecoder elevationDecoder;
    HillshadeRasterTileLayer hillshadeLayer;

    // Function to check and request permission
    @SuppressLint("NewApi")
    public void checkStoragePermission(View view) {

        // Checking if permission is not granted
        if (getActivity().checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED) {
            requestPermissions(
                    new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    REQUEST_PERMISSIONS_CODE_WRITE_STORAGE);

        } else {
            proceedWithSdCard(view);
        }
    }


    public void toggleSlopes(boolean activated) {
        hillshadeLayer.setExagerateHeightScaleEnabled(!activated);
        hillshadeLayer.setNormalMapLightingShader(activated ? "uniform vec4 u_shadowColor;\n" +
                "        uniform vec4 u_highlightColor;\n" +
                "        uniform vec4 u_accentColor;\n" +
                "        uniform vec3 u_lightDir;\n" +
                "        vec4 applyLighting(lowp vec4 color, mediump vec3 normal, mediump vec3 surfaceNormal, mediump float intensity) {\n" +
                "            mediump float lighting = max(0.0, dot(normal, u_lightDir));\n" +
                "            mediump float slope = acos(dot(normal, surfaceNormal)) *180.0 / 3.14159 * 1.2;\n" +
                "            if (slope >= 45.0) {return vec4(0.7568627450980392* 0.5, 0.5450980392156863* 0.5, 0.7176470588235294* 0.5, 0.5); }\n" +
                "            if (slope >= 40.0) {return vec4( 0.5, 0, 0, 0.5); }\n" +
                "            if (slope >= 35.0) {return vec4(0.9098039215686275* 0.5, 0.4627450980392157* 0.5, 0.2235294117647059* 0.5, 0.5); }\n" +
                "            if (slope >= 30.0) {return vec4(0.9411764705882353* 0.5, 0.9019607843137255* 0.5, 0.3058823529411765* 0.5, 0.5); }\n" +
                "            return vec4(0, 0, 0, 0.0);\n" +
                "        }\n" +
                " " : "");
    }

    @RequiresApi(api = Build.VERSION_CODES.O)
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (requestCode == REQUEST_PERMISSIONS_CODE_WRITE_STORAGE) {
            if (permissions[0].equals(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                    && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                if (Build.VERSION.SDK_INT >= 30) {
                    // If you have access to the external storage, do whatever you need
                    if (Environment.isExternalStorageManager()) {

                        // If you don't have access, launch a new activity to show the user the system's dialog
                        // to allow access to the external storage
                    } else {
                        Intent intent = new Intent();
                        intent.setAction(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                        Uri uri = Uri.fromParts("package", getActivity().getPackageName(), null);
                        intent.setData(uri);
                        startActivityForResult(intent, REQUEST_PERMISSIONS_MANAGE_STORAGE);
                    }
                } else {
                    proceedWithSdCard(this.getView());

                }
            }
        } else if (requestCode == REQUEST_PERMISSIONS_CODE_WRITE_STORAGE) {
            proceedWithSdCard(this.getView());

        }
    }
    void addHillshadeLayer(View view, String dataPath) {
        MBTilesTileDataSource hillshadeSourceFrance = null;
        MBTilesTileDataSource hillshadeSourceWorld = null;
        MultiTileDataSource  dataSource = new MultiTileDataSource();
        try {
            hillshadeSourceFrance = new MBTilesTileDataSource( dataPath+"/france_terrain.etiles");
            hillshadeSourceWorld = new MBTilesTileDataSource( dataPath+"/world_terrain.etiles");
//            hillshadeSource = this.hillshadeSource = new HTTPTileDataSource(5, 11, "http://192.168.1.45:8080/data/BDALTIV2_75M_rvb/{z}/{x}/{y}.png");
            //        HTTPTileDataSource hillshadeSource =   new HTTPTileDataSource(1, 15, "https://api.mapbox.com/v4/mapbox.terrain-rgb/{z}/{x}/{y}.pngraw?access_token=pk.eyJ1IjoiYWt5bGFzIiwiYSI6IkVJVFl2OXMifQ.TGtrEmByO3-99hA0EI44Ew");
        } catch (Exception e) {
            e.printStackTrace();
        }
//        hillshadeSourceWorld.setMaxOverzoomLevel(1);
        dataSource.add(hillshadeSourceFrance, "wzAzMzDAwMBwwwXFfBcVXAzAxE8BcVfMxXFXMBFfxVVVwMzMBMVwMB8RVPHFV9QQ1xDUPwMDFfMRwFVzwFXMVxFVUz/MQD1BAPXENQTMQAP1dA1xV9AxExFc1NQDXNQDUxAAAPD/ww3BV8FxVfDAcVwVcwMMFwXwxV8BwVV/FVVV/DMBXwXFV8DBcMFVX/AcEXBV8MED0Q0QH9ENED0Q0AN1fVNQDV1dU/VNQA9EAP1dU11AAB9/RDRAw0QN1dEfBDRcEDfDRcEEdw0QfRR0QP1111FXdXV0DXV1T1TUNAPXRNQA/MQD1FMQDdXRM1EN0DUAP9Q0AAAP3V1DUPUAPXUNA3QAAAAAAAAA%");
//        dataSource.add(hillshadeSourceWorld);
        final MapBoxElevationDataDecoder elevationDecoder = new MapBoxElevationDataDecoder();
        final HillshadeRasterTileLayer layer = hillshadeLayer = new HillshadeRasterTileLayer(hillshadeSourceFrance, elevationDecoder);
        layer.setPreloading(true);
        layer.setContrast( 0.52f);
        layer.setHeightScale(0.32f);
        layer.setVisibleZoomRange(new MapRange(0, 16));
        layer.setIlluminationMapRotationEnabled(true);
        layer.setIlluminationDirection(new MapVec(-1, 0, 0));
        layer.setTileSubstitutionPolicy(TileSubstitutionPolicy.TILE_SUBSTITUTION_POLICY_VISIBLE);
        layer.setTileFilterMode(RasterTileFilterMode.RASTER_TILE_FILTER_MODE_BILINEAR);
        layer.setHighlightColor(new Color((short) 0, (short) 0, (short) 0, (short) 170));
        layer.setShadowColor(new Color((short) 0, (short) 0, (short) 0, (short) 0));
        layer.setAccentColor(new Color((short) 0, (short) 0, (short) 0, (short) 170));

        mapView.getLayers().add(layer);
//        toggleSlopes(true);
        final CheckBox slopesCheckBox = view.findViewById(R.id.slopesCheckBox); // initiate the Seek bar
        slopesCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
                toggleSlopes(b);
            }
        });
        final AppCompatSeekBar contrastSeekBar = (AppCompatSeekBar) view.findViewById(R.id.contrastSeekBar); // initiate the Seek bar
        final TextView textContrast = (TextView) view.findViewById(R.id.textContrast); // initiate the Seek bar
        contrastSeekBar.setProgress((int) (layer.getContrast() * 100.0f));
        textContrast.setText(layer.getContrast() + "");
        contrastSeekBar.setOnSeekBarChangeListener(new AppCompatSeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {

                layer.setContrast(i / 100.0f);
                textContrast.setText(layer.getContrast() + "");
                mapView.requestRender();
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }

        });
        final AppCompatSeekBar illuminationDirectionSeekBar = (AppCompatSeekBar) view.findViewById(R.id.illuminationDirectionSeekBar); // initiate the Seek bar
        final TextView textIlluminationDirection = (TextView) view.findViewById(R.id.textIlluminationDirection); // initiate the Seek bar

        final double degrees = (Math.acos(layer.getIlluminationDirection().getY()) * 180 / Math.PI);
        illuminationDirectionSeekBar.setProgress((int) degrees);
        textIlluminationDirection.setText((int) degrees + "");
        illuminationDirectionSeekBar.setOnSeekBarChangeListener(new AppCompatSeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                MapVec current = layer.getIlluminationDirection();
                double rad = i * Math.PI / 180;
                double sin = Math.sin(rad);
                double cos = Math.cos(rad);
                layer.setIlluminationDirection(new MapVec(sin, cos, current.getZ()));
                textIlluminationDirection.setText(i + "");
                mapView.requestRender();
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }

        });


        final AppCompatSeekBar heightScaleSeekBar = (AppCompatSeekBar) view.findViewById(R.id.heightScaleSeekBar); // initiate the Seek bar
        final TextView textHeightScale = (TextView) view.findViewById(R.id.textHeightScale); // initiate the Seek bar
        heightScaleSeekBar.setProgress((int) (layer.getHeightScale() * 100.0f));
        textHeightScale.setText(layer.getHeightScale() + "");
//        heightScaleSeekBar.setOnSeekBarChangeListener(new AppCompatSeekBar.OnSeekBarChangeListener() {
//            @Override
//            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
//
//                layer.setHeightScale(i / 100.0f);
//                textHeightScale.setText(layer.getHeightScale() + "");
//                mapView.requestRender();
//            }
//
//            @Override
//            public void onStartTrackingTouch(SeekBar seekBar) {
//            }
//
//            @Override
//            public void onStopTrackingTouch(SeekBar seekBar) {
//            }
//
//        });


        final AppCompatSeekBar highlightOpacitySeekBar = (AppCompatSeekBar) view.findViewById(R.id.highlightOpacitySeekBar); // initiate the Seek bar
        final TextView textHighlightOpacity = (TextView) view.findViewById(R.id.testHighlightOpacity); // initiate the Seek bar
        highlightOpacitySeekBar.setProgress(255);
        textHighlightOpacity.setText("255");
        highlightOpacitySeekBar.setOnSeekBarChangeListener(new AppCompatSeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                Color color = layer.getHighlightColor();
                Color highlightColor = new Color(color.getR(), color.getG(), color.getB(), (short) i);
                layer.setHighlightColor(highlightColor);
                textHighlightOpacity.setText(i + "");
                mapView.requestRender();
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }

        });
        final AppCompatSeekBar shadowOpacitySeekBar = (AppCompatSeekBar) view.findViewById(R.id.shadowOpacitySeekBar); // initiate the Seek bar
        final TextView textShadowOpacity = (TextView) view.findViewById(R.id.testShadowOpacity); // initiate the Seek bar
        shadowOpacitySeekBar.setProgress(255);
        textShadowOpacity.setText("255");
        shadowOpacitySeekBar.setOnSeekBarChangeListener(new AppCompatSeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                Color color = layer.getShadowColor();
                Color shadowColor = new Color(color.getR(), color.getG(), color.getB(), (short) i);
                layer.setShadowColor(shadowColor);
                textShadowOpacity.setText(i + "");
                mapView.requestRender();
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }

        });

    }
    VectorTileLayer mainMapLayer;
    void addMap(String dataPath) {
        MultiTileDataSource  dataSource = new MultiTileDataSource();
        MBTilesTileDataSource sourceFrance = null;
        MBTilesTileDataSource sourceItaly = null;
        MBTilesTileDataSource sourceFranceContours = null;
        MBTilesTileDataSource sourceWorld = null;
        MBVectorTileDecoder decoder = null;
        try {
            sourceFrance = new MBTilesTileDataSource( dataPath+"/france/france.mbtiles");
//            sourceItaly = new MBTilesTileDataSource( dataPath+"/netherlands/netherlands.mbtiles");
//            sourceFranceContours = new MBTilesTileDataSource( dataPath+"/france/france_contours.mbtiles");
//            sourceWorld = new MBTilesTileDataSource( dataPath+"/world.mbtiles");france
            final File file = new File(dataPath+"/osm.zip");
            final FileInputStream stream = new java.io.FileInputStream(file);
            final DataInputStream dataInputStream = new java.io.DataInputStream(stream);
            final byte[] bytes = new byte[(int)file.length()];
            dataInputStream.readFully(bytes);
            decoder = new MBVectorTileDecoder(new CompiledStyleSet(new ZippedAssetPackage(new com.carto.core.BinaryData(bytes))));
        } catch (IOException e) {
            e.printStackTrace();
        }

//        MergedMBVTTileDataSource mergedSource = new MergedMBVTTileDataSource(sourceFranceContours, sourceFrance);
//        dataSource.add(sourceFrance);
//        dataSource.add(mergedSource);
//        dataSource.add(sourceItaly);
//        dataSource.add(sourceWorld);
        mainMapLayer  = new VectorTileLayer(sourceFrance, decoder);
        mapView.getLayers().add(mainMapLayer);
    }
    void addRoutes(String dataPath) {
        MultiTileDataSource  dataSource = new MultiTileDataSource();
        MBTilesTileDataSource sourceFrance = null;
        MBTilesTileDataSource sourceWorld = null;
        MBVectorTileDecoder decoder = null;
        try {
            sourceFrance = new MBTilesTileDataSource( dataPath+"/france/france_routes.mbtiles");
//            sourceWorld = new MBTilesTileDataSource( dataPath+"/world_routes_9.mbtiles");
            final File file = new File(dataPath+"/inner.zip");
            final FileInputStream stream = new java.io.FileInputStream(file);
            final DataInputStream dataInputStream = new java.io.DataInputStream(stream);
            final byte[] bytes = new byte[(int)file.length()];
            dataInputStream.readFully(bytes);
            decoder = new MBVectorTileDecoder(new CompiledStyleSet(new ZippedAssetPackage(new com.carto.core.BinaryData(bytes))));
        } catch (IOException e) {
            e.printStackTrace();
        }

//        dataSource.add(sourceWorld);
        dataSource.add(sourceFrance);
        VectorTileLayer backlayer  = new VectorTileLayer(dataSource, decoder);
        mapView.getLayers().add(backlayer);
    }

    @RequiresApi(api = Build.VERSION_CODES.O)
    void proceedWithSdCard(View view) {
        File externalPath = null;
        File[] externalPaths = getContext().getExternalFilesDirs(null);
        if (externalPaths != null && externalPaths.length > 1) {
            externalPath = externalPaths[externalPaths.length - 1];
        }
        if (externalPath == null) {
            externalPath = getContext().getExternalFilesDir(null);
        }
        String dataPath = Paths.get(externalPath.getAbsolutePath(), "../../../../alpimaps_mbtiles").normalize().toString();


        addMap(dataPath);
//        addRoutes(dataPath);
//        addHillshadeLayer(view, dataPath);

//        try {
//            MBTilesTileDataSource dataSource = new MBTilesTileDataSource(dataPath+"/france/france_terrain.etiles");
//            RasterTileLayer layer = new RasterTileLayer(dataSource);
//            mapView.getLayers().add(layer);
//        } catch (IOException e) {
//            e.printStackTrace();
//        }

//        HTTPTileDataSource dataSource = new HTTPTileDataSource(0,19,"https://tile.openstreetmap.org/{z}/{x}/{y}.png");
//        StringMap headers = new StringMap();
//        headers.set("User-Agent", "test app");
//        dataSource.setHTTPHeaders(headers);
//        PersistentCacheTileDataSource pDataSource = new PersistentCacheTileDataSource(dataSource, getContext().getExternalFilesDir(null) + "/mapcache.db");
//        RasterTileLayer layer = new RasterTileLayer(pDataSource);
//        mapView.getLayers().add(layer);


//        testLineDrawing();
//        testLineDrawing2();
        final TextView textZoom = (TextView) view.findViewById(R.id.zoomText); // initiate the Seek bar
        mapView.setMapEventListener(new MapEventListener() {
            @Override
            public void onMapMoved() {
                super.onMapMoved();
                getActivity().runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        textZoom.setText(String.format("z=%.2f", mapView.getZoom()));
                    }
                });
            }

            @Override
            public void onMapClicked(MapClickInfo mapClickInfo) {
                super.onMapClicked(mapClickInfo);
                MapPos clickPos = mapClickInfo.getClickPos();
//                Log.d(TAG, "elevation " + layer.getElevation(new MapPos(5.722772489758224, 45.182362864932706)));
            }
        });

        final Options options = mapView.getOptions();

        final Button modeButton = (Button) view.findViewById(R.id.modeButton); // initiate the Seek bar
        modeButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                    // testVectorTileSearch("pub");
               testValhallaBicycle(dataPath, options);
//                testMatchRoute(options);
//                testVectoTileSearch(backlayer, options);
                // Code here executes on main thread after user presses button
//                if (options.getRenderProjectionMode() == RenderProjectionMode.RENDER_PROJECTION_MODE_SPHERICAL) {
//                    options.setRenderProjectionMode(RenderProjectionMode.RENDER_PROJECTION_MODE_PLANAR);
//                } else {
//                    options.setRenderProjectionMode(RenderProjectionMode.RENDER_PROJECTION_MODE_SPHERICAL);
//                }
            }
        });
    }

    public void testLineDrawing() {

        String geojson = "{\"type\": \"FeatureCollection\", \"features\":[{\"type\":\"Feature\",\"id\":1668607642215,\"properties\":{\"name\":\"Bastille - 45.192, 5.726\",\"class\":\"pedestrian\",\"id\":1668607642215,\"zoomBounds\":{\"southwest\":{\"lat\":45.191462,\"lon\":5.719694,\"altitude\":0},\"northeast\":{\"lat\":45.198495,\"lon\":5.725541,\"altitude\":0}},\"route\":{\"costing_options\":{\"pedestrian\":{\"use_ferry\":0,\"shortest\":false,\"driveway_factor\":200,\"walkway_factor\":0.8,\"use_tracks\":1,\"sidewalk_factor\":10,\"max_hiking_difficulty\":3}},\"totalTime\":1452.0199999999995,\"totalDistance\":1994,\"type\":\"pedestrian\",\"subtype\":\"normal\"}},\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[5.724991999999999,45.198263999999995],[5.724781,45.198305999999995],[5.724622999999999,45.198335],[5.724584999999999,45.198342],[5.724461,45.198384],[5.724422,45.198426],[5.724425,45.198446999999994],[5.72432,45.198468],[5.7242869999999995,45.198476],[5.724237,45.198479],[5.724199,45.198481],[5.7240899999999995,45.198488999999995],[5.724057999999999,45.198491999999995],[5.724011,45.198495],[5.724009,45.198485],[5.724072,45.198479999999996],[5.7240709999999995,45.19847],[5.723933,45.198478],[5.723930999999999,45.198467],[5.723980999999999,45.198463],[5.723978,45.198451999999996],[5.72391,45.198451999999996],[5.723897,45.19845],[5.723898,45.198442],[5.723902,45.198425],[5.723916,45.198375],[5.723934,45.198370999999995],[5.723947,45.198329],[5.723929,45.198327],[5.723917,45.198364],[5.723875,45.198358999999996],[5.723752999999999,45.198343],[5.723758,45.198324],[5.723777999999999,45.198251],[5.723794,45.198175],[5.723775,45.198163],[5.723751,45.198156999999995],[5.723688999999999,45.198175],[5.723624,45.198203],[5.7235759999999996,45.198215999999995],[5.7234989999999994,45.198217],[5.723252,45.198201999999995],[5.7232389999999995,45.198175],[5.723484,45.198088],[5.7234989999999994,45.198119999999996],[5.723387,45.198156999999995],[5.723345999999999,45.198114],[5.723256999999999,45.198051],[5.723043,45.198113],[5.722871,45.198204999999994],[5.722697,45.198249],[5.722523,45.198312],[5.722471,45.198361],[5.722366999999999,45.198356],[5.7223559999999996,45.198339999999995],[5.722421,45.198307],[5.722478,45.198291999999995],[5.722595999999999,45.19826],[5.722585,45.198246],[5.722563999999999,45.198226999999996],[5.72253,45.198195],[5.722509,45.198187999999995],[5.722351,45.198212999999996],[5.7223679999999995,45.198153],[5.722252,45.198141],[5.72222,45.198088],[5.722366,45.198060999999996],[5.722497,45.198046999999995],[5.722523,45.198026999999996],[5.722532999999999,45.197995999999996],[5.722532999999999,45.197987],[5.722532999999999,45.197981],[5.722594,45.197981],[5.722633999999999,45.197981999999996],[5.722633999999999,45.197981],[5.722624,45.197979],[5.722621,45.197976],[5.722636,45.197970999999995],[5.722601999999999,45.197961],[5.722631,45.197919],[5.722544,45.197950999999996],[5.7225909999999995,45.197888],[5.722498,45.197922999999996],[5.722549,45.197873],[5.722493999999999,45.197897999999995],[5.722448,45.197913],[5.722453,45.197899],[5.72251,45.197852],[5.722411999999999,45.197888],[5.722428,45.197858],[5.722449999999999,45.197828],[5.722411,45.197846999999996],[5.722392999999999,45.197859],[5.722364,45.197858],[5.7223559999999996,45.197838999999995],[5.722366999999999,45.197821999999995],[5.722421,45.197796],[5.722497,45.197755],[5.722506,45.197739999999996],[5.722512,45.197722],[5.722515,45.197705],[5.722499,45.1977],[5.722484,45.197708999999996],[5.722442,45.197725],[5.722403,45.197724],[5.722395,45.197705],[5.722404999999999,45.197649999999996],[5.722398,45.197581],[5.72241,45.19753],[5.7224319999999995,45.197483],[5.722421,45.197409],[5.722417,45.197331999999996],[5.722405999999999,45.197237],[5.722396,45.197148999999996],[5.722318,45.197112999999995],[5.7222219999999995,45.197075999999996],[5.722169,45.197125],[5.722085,45.197209],[5.722022,45.197269],[5.722009,45.197210999999996],[5.721998999999999,45.197136],[5.722036,45.197055],[5.722083,45.19701],[5.722144,45.196959],[5.7222159999999995,45.196928],[5.722327,45.196873],[5.722449,45.196833],[5.722517,45.196781],[5.722595999999999,45.196709999999996],[5.72264,45.196687999999995],[5.722669,45.196639999999995],[5.722707,45.196518999999995],[5.722725,45.196402],[5.722696,45.196284],[5.722702,45.196157],[5.722684,45.196053],[5.722614999999999,45.195842999999996],[5.722516,45.195705],[5.722417999999999,45.195591],[5.722369,45.195533],[5.722322999999999,45.195437],[5.722347999999999,45.195391],[5.72235,45.195353],[5.722313,45.195299],[5.722179,45.195167],[5.722080999999999,45.195015],[5.721939,45.194826],[5.721836,45.194733],[5.721722,45.194655],[5.721741,45.194632],[5.721741,45.194614],[5.721667,45.194519],[5.72166,45.194509],[5.721617,45.194525999999996],[5.721609,45.19452],[5.721653,45.1945],[5.721645,45.194489999999995],[5.7215929999999995,45.194514],[5.72122,45.194590999999996],[5.721182,45.194579],[5.721137,45.194576],[5.721083,45.194593999999995],[5.721041,45.194627999999994],[5.721,45.19464],[5.720738,45.194770999999996],[5.720686,45.194803],[5.720632,45.194778],[5.720625999999999,45.194773999999995],[5.7206209999999995,45.19477],[5.72052,45.194811],[5.720431,45.194809],[5.720349,45.194773],[5.720244999999999,45.194638999999995],[5.720203,45.194590999999996],[5.720161,45.194579],[5.720134,45.194562999999995],[5.720116,45.194541],[5.720111999999999,45.194488],[5.72009,45.194468],[5.7200359999999995,45.194455999999995],[5.720003,45.194438999999996],[5.7199979999999995,45.194403],[5.7199789999999995,45.194387],[5.7199279999999995,45.194331],[5.7198709999999995,45.194317999999996],[5.719755999999999,45.194291],[5.719694,45.194244999999995],[5.7197059999999995,45.194187],[5.719761999999999,45.194167],[5.71981,45.194213],[5.719823,45.194216],[5.719863999999999,45.194224],[5.719895999999999,45.194237],[5.719936,45.194199],[5.719949,45.194191],[5.7199409999999995,45.194181],[5.719945,45.194168],[5.719965999999999,45.194168],[5.720041,45.194181],[5.72011,45.194143],[5.720148,45.194072],[5.720155999999999,45.194005999999995],[5.720142999999999,45.193894],[5.720135,45.193822999999995],[5.720127,45.193774999999995],[5.720072,45.193737999999996],[5.720044,45.193673],[5.7200679999999995,45.193611],[5.720184,45.193559],[5.720178,45.193509],[5.720289999999999,45.193508],[5.720431,45.193506],[5.721293999999999,45.19354],[5.721585,45.193549],[5.721619,45.193549999999995],[5.723077,45.193596],[5.723552,45.193624],[5.72358,45.193591],[5.723805,45.193608999999995],[5.723789,45.193574],[5.723806,45.193525],[5.72385,45.193526999999996],[5.723913,45.193535999999995],[5.723964,45.193543999999996],[5.723967,45.19352],[5.72419,45.192761],[5.7241979999999995,45.192735],[5.724209,45.192727999999995],[5.7242239999999995,45.192671],[5.724235999999999,45.192616],[5.724412999999999,45.19265],[5.724696,45.192709],[5.724729,45.192715],[5.724765,45.192721999999996],[5.724876999999999,45.19231],[5.724895,45.192285999999996],[5.724959,45.192254],[5.7250179999999995,45.192218],[5.725121,45.191857999999996],[5.725156,45.191828],[5.725169999999999,45.191769],[5.725185,45.191704],[5.725249,45.191478],[5.725264,45.191466],[5.725541,45.191462]]}}]}";
        String cartoCss =
                "#items[zoom > 'nuti::test'] {\n" +
                        "  line-color: #374C70;\n" +
                        "  line-cap: round;\n" +
                        "  line-join: round;\n" +
                        "  line-width: 12;\n" +
                        "}\n" ;

        MBVectorTileDecoder decoder = new MBVectorTileDecoder(new CartoCSSStyleSet(cartoCss));
        decoder.setStyleParameter("test", "10");
        GeoJSONVectorTileDataSource dataSource = new GeoJSONVectorTileDataSource(0,24);
        try {
            dataSource.createLayer("items");
            dataSource.addGeoJSONStringFeature(1, "{\"type\":\"Feature\",\"id\":1668607642215,\"properties\":{\"name\":\"Bastille - 45.192, 5.726\",\"class\":\"pedestrian\",\"id\":1668607642215,\"zoomBounds\":{\"southwest\":{\"lat\":45.191462,\"lon\":5.719694,\"altitude\":0},\"northeast\":{\"lat\":45.198495,\"lon\":5.725541,\"altitude\":0}},\"route\":{\"costing_options\":{\"pedestrian\":{\"use_ferry\":0,\"shortest\":false,\"driveway_factor\":200,\"walkway_factor\":0.8,\"use_tracks\":1,\"sidewalk_factor\":10,\"max_hiking_difficulty\":3}},\"totalTime\":1452.0199999999995,\"totalDistance\":1994,\"type\":\"pedestrian\",\"subtype\":\"normal\"}},\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[5.7223559999999996,45.198339999999995],[5.722421,45.198307],[5.722478,45.198291999999995],[5.722595999999999,45.19826],[5.722585,45.198246],[5.722563999999999,45.198226999999996],[5.72253,45.198195],[5.722509,45.198187999999995],[5.722351,45.198212999999996],[5.7223679999999995,45.198153],[5.722252,45.198141],[5.72222,45.198088],[5.722366,45.198060999999996],[5.722497,45.198046999999995],[5.722523,45.198026999999996],[5.722532999999999,45.197995999999996],[5.722532999999999,45.197987],[5.722532999999999,45.197981],[5.722594,45.197981],[5.722633999999999,45.197981999999996],[5.722633999999999,45.197981],[5.722624,45.197979],[5.722621,45.197976],[5.722636,45.197970999999995],[5.722601999999999,45.197961],[5.722631,45.197919],[5.722544,45.197950999999996],[5.7225909999999995,45.197888],[5.722498,45.197922999999996],[5.722549,45.197873],[5.722493999999999,45.197897999999995],[5.722448,45.197913],[5.722453,45.197899],[5.72251,45.197852],[5.722411999999999,45.197888],[5.722428,45.197858],[5.722449999999999,45.197828],[5.722411,45.197846999999996],[5.722392999999999,45.197859],[5.722364,45.197858],[5.7223559999999996,45.197838999999995],[5.722366999999999,45.197821999999995],[5.722421,45.197796],[5.722497,45.197755],[5.722506,45.197739999999996],[5.722512,45.197722],[5.722515,45.197705],[5.722499,45.1977],[5.722484,45.197708999999996],[5.722442,45.197725],[5.722403,45.197724],[5.722395,45.197705],[5.722404999999999,45.197649999999996],[5.722398,45.197581],[5.72241,45.19753],[5.7224319999999995,45.197483],[5.722421,45.197409],[5.722417,45.197331999999996],[5.722405999999999,45.197237],[5.722396,45.197148999999996],[5.722318,45.197112999999995],[5.7222219999999995,45.197075999999996],[5.722169,45.197125],[5.722085,45.197209],[5.722022,45.197269],[5.722009,45.197210999999996],[5.721998999999999,45.197136],[5.722036,45.197055],[5.722083,45.19701],[5.722144,45.196959],[5.7222159999999995,45.196928],[5.722327,45.196873],[5.722449,45.196833],[5.722517,45.196781],[5.722595999999999,45.196709999999996],[5.72264,45.196687999999995],[5.722669,45.196639999999995],[5.722707,45.196518999999995],[5.722725,45.196402],[5.722696,45.196284],[5.722702,45.196157],[5.722684,45.196053],[5.722614999999999,45.195842999999996],[5.722516,45.195705],[5.722417999999999,45.195591],[5.722369,45.195533],[5.722322999999999,45.195437],[5.722347999999999,45.195391],[5.72235,45.195353],[5.722313,45.195299],[5.722179,45.195167],[5.722080999999999,45.195015],[5.721939,45.194826],[5.721836,45.194733],[5.721722,45.194655],[5.721741,45.194632],[5.721741,45.194614],[5.721667,45.194519],[5.72166,45.194509],[5.721617,45.194525999999996],[5.721609,45.19452],[5.721653,45.1945],[5.721645,45.194489999999995],[5.7215929999999995,45.194514],[5.72122,45.194590999999996],[5.721182,45.194579],[5.721137,45.194576],[5.721083,45.194593999999995],[5.721041,45.194627999999994],[5.721,45.19464],[5.720738,45.194770999999996],[5.720686,45.194803],[5.720632,45.194778],[5.720625999999999,45.194773999999995],[5.7206209999999995,45.19477],[5.72052,45.194811],[5.720431,45.194809],[5.720349,45.194773],[5.720244999999999,45.194638999999995],[5.720203,45.194590999999996],[5.720161,45.194579],[5.720134,45.194562999999995],[5.720116,45.194541],[5.720111999999999,45.194488],[5.72009,45.194468],[5.7200359999999995,45.194455999999995],[5.720003,45.194438999999996],[5.7199979999999995,45.194403],[5.7199789999999995,45.194387],[5.7199279999999995,45.194331],[5.7198709999999995,45.194317999999996],[5.719755999999999,45.194291],[5.719694,45.194244999999995],[5.7197059999999995,45.194187],[5.719761999999999,45.194167],[5.71981,45.194213],[5.719823,45.194216],[5.719863999999999,45.194224],[5.719895999999999,45.194237],[5.719936,45.194199],[5.719949,45.194191],[5.7199409999999995,45.194181],[5.719945,45.194168],[5.719965999999999,45.194168],[5.720041,45.194181],[5.72011,45.194143],[5.720148,45.194072],[5.720155999999999,45.194005999999995],[5.720142999999999,45.193894],[5.720135,45.193822999999995],[5.720127,45.193774999999995],[5.720072,45.193737999999996],[5.720044,45.193673],[5.7200679999999995,45.193611],[5.720184,45.193559],[5.720178,45.193509],[5.720289999999999,45.193508],[5.720431,45.193506],[5.721293999999999,45.19354],[5.721585,45.193549],[5.721619,45.193549999999995],[5.723077,45.193596],[5.723552,45.193624],[5.72358,45.193591],[5.723805,45.193608999999995],[5.723789,45.193574],[5.723806,45.193525],[5.72385,45.193526999999996],[5.723913,45.193535999999995],[5.723964,45.193543999999996],[5.723967,45.19352],[5.72419,45.192761],[5.7241979999999995,45.192735],[5.724209,45.192727999999995],[5.7242239999999995,45.192671],[5.724235999999999,45.192616],[5.724412999999999,45.19265],[5.724696,45.192709],[5.724729,45.192715],[5.724765,45.192721999999996],[5.724876999999999,45.19231],[5.724895,45.192285999999996],[5.724959,45.192254],[5.7250179999999995,45.192218],[5.725121,45.191857999999996],[5.725156,45.191828],[5.725169999999999,45.191769],[5.725185,45.191704],[5.725249,45.191478],[5.725264,45.191466],[5.725541,45.191462]]}}");
            dataSource.addGeoJSONStringFeature(1, "{\"type\":\"Feature\",\"id\":1668607642235,\"properties\":{\"name\":\"Bastille - 45.192, 5.726\",\"class\":\"pedestrian\",\"id\":1668607642235,\"zoomBounds\":{\"southwest\":{\"lat\":45.191462,\"lon\":5.719694,\"altitude\":0},\"northeast\":{\"lat\":45.198495,\"lon\":5.725541,\"altitude\":0}},\"route\":{\"costing_options\":{\"pedestrian\":{\"use_ferry\":0,\"shortest\":false,\"driveway_factor\":200,\"walkway_factor\":0.8,\"use_tracks\":1,\"sidewalk_factor\":10,\"max_hiking_difficulty\":3}},\"totalTime\":1452.0199999999995,\"totalDistance\":1994,\"type\":\"pedestrian\",\"subtype\":\"normal\"}},\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[5.724991999999999,45.198263999999995],[5.724781,45.198305999999995],[5.724622999999999,45.198335],[5.724584999999999,45.198342],[5.724461,45.198384],[5.724422,45.198426],[5.724425,45.198446999999994],[5.72432,45.198468],[5.7242869999999995,45.198476],[5.724237,45.198479],[5.724199,45.198481],[5.7240899999999995,45.198488999999995],[5.724057999999999,45.198491999999995]]}}");
//            dataSource.setLayerGeoJSON(1,com.carto.core.Variant.fromString(geojson));
            dataSource.removeGeoJSONFeature(1, new Variant(1668607642235L));
            VectorTileLayer mbLayer = new VectorTileLayer(dataSource,decoder);
            mapView.getLayers().add(mbLayer);
        } catch (IOException e) {
            e.printStackTrace();
        }

        mapView.setFocusPos(new MapPos(5.72476358599884, 45.19272038067931), 0);
        mapView.setZoom(15f, 0);
    }


    public void testLineDrawing2() {
        final Options options = mapView.getOptions();

        String geojson = "{\"type\": \"FeatureCollection\", \"features\":[{\"type\":\"Feature\",\"id\":1668607642215,\"properties\":{\"name\":\"Bastille - 45.192, 5.726\",\"class\":\"pedestrian\",\"id\":1668607642215,\"zoomBounds\":{\"southwest\":{\"lat\":45.191462,\"lon\":5.719694,\"altitude\":0},\"northeast\":{\"lat\":45.198495,\"lon\":5.725541,\"altitude\":0}},\"route\":{\"costing_options\":{\"pedestrian\":{\"use_ferry\":0,\"shortest\":false,\"driveway_factor\":200,\"walkway_factor\":0.8,\"use_tracks\":1,\"sidewalk_factor\":10,\"max_hiking_difficulty\":3}},\"totalTime\":1452.0199999999995,\"totalDistance\":1994,\"type\":\"pedestrian\",\"subtype\":\"normal\"}},\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[5.724991999999999,45.198263999999995],[5.724781,45.198305999999995],[5.724622999999999,45.198335],[5.724584999999999,45.198342],[5.724461,45.198384],[5.724422,45.198426],[5.724425,45.198446999999994],[5.72432,45.198468],[5.7242869999999995,45.198476],[5.724237,45.198479],[5.724199,45.198481],[5.7240899999999995,45.198488999999995],[5.724057999999999,45.198491999999995],[5.724011,45.198495],[5.724009,45.198485],[5.724072,45.198479999999996],[5.7240709999999995,45.19847],[5.723933,45.198478],[5.723930999999999,45.198467],[5.723980999999999,45.198463],[5.723978,45.198451999999996],[5.72391,45.198451999999996],[5.723897,45.19845],[5.723898,45.198442],[5.723902,45.198425],[5.723916,45.198375],[5.723934,45.198370999999995],[5.723947,45.198329],[5.723929,45.198327],[5.723917,45.198364],[5.723875,45.198358999999996],[5.723752999999999,45.198343],[5.723758,45.198324],[5.723777999999999,45.198251],[5.723794,45.198175],[5.723775,45.198163],[5.723751,45.198156999999995],[5.723688999999999,45.198175],[5.723624,45.198203],[5.7235759999999996,45.198215999999995],[5.7234989999999994,45.198217],[5.723252,45.198201999999995],[5.7232389999999995,45.198175],[5.723484,45.198088],[5.7234989999999994,45.198119999999996],[5.723387,45.198156999999995],[5.723345999999999,45.198114],[5.723256999999999,45.198051],[5.723043,45.198113],[5.722871,45.198204999999994],[5.722697,45.198249],[5.722523,45.198312],[5.722471,45.198361],[5.722366999999999,45.198356],[5.7223559999999996,45.198339999999995],[5.722421,45.198307],[5.722478,45.198291999999995],[5.722595999999999,45.19826],[5.722585,45.198246],[5.722563999999999,45.198226999999996],[5.72253,45.198195],[5.722509,45.198187999999995],[5.722351,45.198212999999996],[5.7223679999999995,45.198153],[5.722252,45.198141],[5.72222,45.198088],[5.722366,45.198060999999996],[5.722497,45.198046999999995],[5.722523,45.198026999999996],[5.722532999999999,45.197995999999996],[5.722532999999999,45.197987],[5.722532999999999,45.197981],[5.722594,45.197981],[5.722633999999999,45.197981999999996],[5.722633999999999,45.197981],[5.722624,45.197979],[5.722621,45.197976],[5.722636,45.197970999999995],[5.722601999999999,45.197961],[5.722631,45.197919],[5.722544,45.197950999999996],[5.7225909999999995,45.197888],[5.722498,45.197922999999996],[5.722549,45.197873],[5.722493999999999,45.197897999999995],[5.722448,45.197913],[5.722453,45.197899],[5.72251,45.197852],[5.722411999999999,45.197888],[5.722428,45.197858],[5.722449999999999,45.197828],[5.722411,45.197846999999996],[5.722392999999999,45.197859],[5.722364,45.197858],[5.7223559999999996,45.197838999999995],[5.722366999999999,45.197821999999995],[5.722421,45.197796],[5.722497,45.197755],[5.722506,45.197739999999996],[5.722512,45.197722],[5.722515,45.197705],[5.722499,45.1977],[5.722484,45.197708999999996],[5.722442,45.197725],[5.722403,45.197724],[5.722395,45.197705],[5.722404999999999,45.197649999999996],[5.722398,45.197581],[5.72241,45.19753],[5.7224319999999995,45.197483],[5.722421,45.197409],[5.722417,45.197331999999996],[5.722405999999999,45.197237],[5.722396,45.197148999999996],[5.722318,45.197112999999995],[5.7222219999999995,45.197075999999996],[5.722169,45.197125],[5.722085,45.197209],[5.722022,45.197269],[5.722009,45.197210999999996],[5.721998999999999,45.197136],[5.722036,45.197055],[5.722083,45.19701],[5.722144,45.196959],[5.7222159999999995,45.196928],[5.722327,45.196873],[5.722449,45.196833],[5.722517,45.196781],[5.722595999999999,45.196709999999996],[5.72264,45.196687999999995],[5.722669,45.196639999999995],[5.722707,45.196518999999995],[5.722725,45.196402],[5.722696,45.196284],[5.722702,45.196157],[5.722684,45.196053],[5.722614999999999,45.195842999999996],[5.722516,45.195705],[5.722417999999999,45.195591],[5.722369,45.195533],[5.722322999999999,45.195437],[5.722347999999999,45.195391],[5.72235,45.195353],[5.722313,45.195299],[5.722179,45.195167],[5.722080999999999,45.195015],[5.721939,45.194826],[5.721836,45.194733],[5.721722,45.194655],[5.721741,45.194632],[5.721741,45.194614],[5.721667,45.194519],[5.72166,45.194509],[5.721617,45.194525999999996],[5.721609,45.19452],[5.721653,45.1945],[5.721645,45.194489999999995],[5.7215929999999995,45.194514],[5.72122,45.194590999999996],[5.721182,45.194579],[5.721137,45.194576],[5.721083,45.194593999999995],[5.721041,45.194627999999994],[5.721,45.19464],[5.720738,45.194770999999996],[5.720686,45.194803],[5.720632,45.194778],[5.720625999999999,45.194773999999995],[5.7206209999999995,45.19477],[5.72052,45.194811],[5.720431,45.194809],[5.720349,45.194773],[5.720244999999999,45.194638999999995],[5.720203,45.194590999999996],[5.720161,45.194579],[5.720134,45.194562999999995],[5.720116,45.194541],[5.720111999999999,45.194488],[5.72009,45.194468],[5.7200359999999995,45.194455999999995],[5.720003,45.194438999999996],[5.7199979999999995,45.194403],[5.7199789999999995,45.194387],[5.7199279999999995,45.194331],[5.7198709999999995,45.194317999999996],[5.719755999999999,45.194291],[5.719694,45.194244999999995],[5.7197059999999995,45.194187],[5.719761999999999,45.194167],[5.71981,45.194213],[5.719823,45.194216],[5.719863999999999,45.194224],[5.719895999999999,45.194237],[5.719936,45.194199],[5.719949,45.194191],[5.7199409999999995,45.194181],[5.719945,45.194168],[5.719965999999999,45.194168],[5.720041,45.194181],[5.72011,45.194143],[5.720148,45.194072],[5.720155999999999,45.194005999999995],[5.720142999999999,45.193894],[5.720135,45.193822999999995],[5.720127,45.193774999999995],[5.720072,45.193737999999996],[5.720044,45.193673],[5.7200679999999995,45.193611],[5.720184,45.193559],[5.720178,45.193509],[5.720289999999999,45.193508],[5.720431,45.193506],[5.721293999999999,45.19354],[5.721585,45.193549],[5.721619,45.193549999999995],[5.723077,45.193596],[5.723552,45.193624],[5.72358,45.193591],[5.723805,45.193608999999995],[5.723789,45.193574],[5.723806,45.193525],[5.72385,45.193526999999996],[5.723913,45.193535999999995],[5.723964,45.193543999999996],[5.723967,45.19352],[5.72419,45.192761],[5.7241979999999995,45.192735],[5.724209,45.192727999999995],[5.7242239999999995,45.192671],[5.724235999999999,45.192616],[5.724412999999999,45.19265],[5.724696,45.192709],[5.724729,45.192715],[5.724765,45.192721999999996],[5.724876999999999,45.19231],[5.724895,45.192285999999996],[5.724959,45.192254],[5.7250179999999995,45.192218],[5.725121,45.191857999999996],[5.725156,45.191828],[5.725169999999999,45.191769],[5.725185,45.191704],[5.725249,45.191478],[5.725264,45.191466],[5.725541,45.191462]]}}]}";

        GeoJSONGeometryReader reader = new GeoJSONGeometryReader();
        reader.setTargetProjection(options.getBaseProjection());
        FeatureCollection collection = reader.readFeatureCollection(geojson);

        GeoJSONVectorTileDataSource dataSource = new GeoJSONVectorTileDataSource(0,24);

        LineStyleBuilder builder = new LineStyleBuilder();
        builder.setWidth(9);
        builder.setColor(new Color((short) 255, (short)0,(short)0,(short)255));
        builder.setLineJoinType(LineJoinType.LINE_JOIN_TYPE_MITER);
        builder.setColor(new Color((short) 255, (short) 0, (short) 0, (short) 255));
        Line line = new Line((LineGeometry) collection.getFeature(0).getGeometry(), builder.buildStyle());

        LocalVectorDataSource localSource = new LocalVectorDataSource(options.getBaseProjection());
        VectorLayer vectorLayer = new VectorLayer(localSource);
        mapView.getLayers().add(vectorLayer);
        localSource.add(line);

        mapView.setFocusPos(new MapPos(5.72476358599884, 45.19272038067931), 0);
        mapView.setZoom(15f, 0);
    }


    public void testVectoTileSearch(final VectorTileLayer layer, final Options options) {

        final VectorTileSearchService searchService = new VectorTileSearchService(layer.getDataSource(), layer.getTileDecoder());
        searchService.setMaxZoom(14);
        searchService.setMinZoom(14);
        MapPosVector vector = new MapPosVector();
        vector.add(new MapPos(5.853,45.096));
        vector.add(new MapPos(6.009,45.225));
        PolygonGeometry boundsGeo = new PolygonGeometry(vector);
        final SearchRequest request = new SearchRequest();
        request.setFilterExpression("layer::name='route' AND osmid=-2535348");
        request.setGeometry(boundsGeo);
        request.setProjection(options.getBaseProjection());
        new Thread() {
            public void run() {
                final VectorTileFeatureCollection result = searchService.findFeatures(request);
                MapPosVectorVector points = new MapPosVectorVector();
                for (int i=0; i < result.getFeatureCount(); i++){
                    Feature feature = result.getFeature(i);
                    Geometry geometry = feature.getGeometry();
                    if (geometry instanceof MultiLineGeometry) {
                        for (int j=0; j < ((MultiLineGeometry)geometry).getGeometryCount(); j++) {
                            points.add(((MultiLineGeometry) geometry).getGeometry(j).getPoses());
                        }
                    } else {
                        points.add(((LineGeometry)geometry).getPoses());
                    }
                }
//                LineStyleBuilder builder = new LineStyleBuilder();
//                builder.setWidth(4);
//                builder.setColor(new Color((short) 255, (short) 0, (short) 0, (short) 255));
//                Line line = new Line(points, builder.buildStyle());
//                localSource.add(line);
                new Handler(android.os.Looper.getMainLooper()).post(new Runnable() {
                    @Override
                    public void run() {
                        Log.d(TAG, "findFeatures done " + result.getFeatureCount());
                    }

                });
            }
        }.start();
    }


    public void runValhallaInThread(final MultiValhallaOfflineRoutingService routingService, final RoutingRequest request, String profile, final LocalVectorDataSource localSource) {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    routingService.setProfile(profile);
                    RoutingResult result = routingService.calculateRoute(request);
                    Log.d(TAG,"rawresult "+ result.getRawResult());

//                    MapPosVector pointsWithAltitude = new MapPosVector();
                    if (result != null) {
                        MapPosVector points = result.getPoints();
                        Log.d(TAG, "showing route " + points.size());
                        LineStyleBuilder builder = new LineStyleBuilder();
                        builder.setWidth(4);
                        builder.setColor(new Color((short) 255, (short) 0, (short) 0, (short) 255));
                        Line line = new Line(points, builder.buildStyle());

                        localSource.add(line);
                        MapBounds bounds = line.getGeometry().getBounds();
                        mapView.moveToFitBounds(bounds, new ScreenBounds(), false, 10);
                    }
//                    RouteMatchingRequest matchrequest = new RouteMatchingRequest(request.getProjection(), result.getPoints(), 1);
//                    matchrequest.setCustomParameter("shape_match", new Variant("edge_walk"));
//                    matchrequest.setCustomParameter("filters", Variant.fromString("{ \"attributes\": [\"edge.surface\", \"edge.road_class\", \"edge.sac_scale\", \"edge.use\", \"edge.length\"], \"action\": \"include\" }"));
//                    RouteMatchingResult matchresult = routingService.matchRoute(matchrequest);
//                    largeLog(TAG,"matchresult "+ matchresult.getRawResult());
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });
        thread.start();
    }
//
//    public void matchRouteTest(final MultiValhallaOfflineRoutingService routingService, Projection projection, String profile) {
//        Thread thread = new Thread(new Runnable() {
//            @Override
//            public void run() {
//                try {
//                    routingService.setProfile(profile);
//                    MapPosVector vector = new MapPosVector();
//                    vector.add(new MapPos(5.721619, 45.193549999999995));
//                    vector.add(new MapPos(5.721585, 45.193549));
//                    RouteMatchingRequest matchrequest = new RouteMatchingRequest(projection, vector, 1);
//                    matchrequest.setCustomParameter("shape_match", new Variant("edge_walk"));
//                    matchrequest.setCustomParameter("filters", Variant.fromString("{ \"attributes\": [\"edge.surface\", \"edge.road_class\", \"edge.sac_scale\", \"edge.use\"], \"action\": \"include\" }"));
//                    RouteMatchingResult matchresult = routingService.matchRoute(matchrequest);
//                    largeLog(TAG,"matchresult "+ matchresult.getRawResult());
//                } catch (IOException e) {
//                    e.printStackTrace();
//                }
//            }
//        });
//        thread.start();
//    }
//
//        public void testMatchRoute(String dataPath, Options options) {
//            Projection projection = options.getBaseProjection();
//            MultiValhallaOfflineRoutingService routingService = new MultiValhallaOfflineRoutingService();
//            routingService.add(dataPath+"/france.vtiles");
//            LocalVectorDataSource localSource = new LocalVectorDataSource(projection);
//            Timer timer = new Timer();
//            TimerTask task = new MathRouteTask(routingService, options.getBaseProjection(), "pedestrian");
//            timer.schedule(task, 0, 500);
//        }
//
//    public void testValhalla(String dataPath, Options options) {
//        Projection projection = options.getBaseProjection();
//        MultiValhallaOfflineRoutingService routingService = new MultiValhallaOfflineRoutingService();
////        routingService.add(dataPath+"/italy/italy.vtiles");
//        routingService.add(dataPath+"/france/france.vtiles");
//        LocalVectorDataSource localSource = new LocalVectorDataSource(projection);
//        VectorLayer vectorLayer = new VectorLayer(localSource);
//        mapView.getLayers().add(vectorLayer);
//        MapPosVector vector = new MapPosVector();
//        vector.add(new MapPos(5.7233, 45.1924));
//        vector.add(new MapPos(5.7247, 45.1992));
//        RoutingRequest request = new RoutingRequest(projection, vector);
//        request.setCustomParameter("costing_options", Variant.fromString("{\"pedestrian\":{\"use_ferry\":0,\"shortest\":false,\"use_hills\":1,\"max_hiking_difficulty\":6,\"step_penalty\":10,\"driveway_factor\":200,\"use_roads\":0,\"use_tracks\":1,\"walking_speed\":4,\"sidewalk_factor\":10}}"));
//        request.setCustomParameter("directions_options", Variant.fromString("{\"language\":\"en\"}"));
//        runValhallaInThread(routingService, request, "pedestrian", localSource);
////            request = new RoutingRequest(projection, vector);
////            request.setCustomParameter("costing_options", Variant.fromString("{\"pedestrian\":{\"driveway_factor\":10,\"max_hiking_difficulty\":6,\"shortest\":false,\"step_penalty\":1,\"use_ferry\":0,\"use_hills\":0,\"use_roads\":0,\"use_tracks\":1,\"walking_speed\":4}},\"directions_options\":{\"language\":\"en\"}}"));
////            runValhallaInThread(routingService, request, "pedestrian", localSource);
////            request = new RoutingRequest(projection, vector);
////            request.setCustomParameter("costing_options", Variant.fromString("{\"pedestrian\":{\"driveway_factor\":10,\"max_hiking_difficulty\":6,\"shortest\":true,\"step_penalty\":5,\"use_ferry\":0,\"use_hills\":1,\"use_roads\":0,\"use_tracks\":1,\"walking_speed\":4}},\"directions_options\":{\"language\":\"en\"}}"));
////            runValhallaInThread(routingService, request, "pedestrian", localSource);
//    }

    public void testVectorTileSearch(String query) {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                long startTime = System.nanoTime();
                SearchRequest request = new SearchRequest();
                request.setFilterExpression("regexp_ilike(name,'.*" + query + ".*') OR regexp_ilike(class,'.*" + query + ".*')");
                request.setSearchRadius(2000);
                request.setGeometry(new PointGeometry(mapView.getFocusPos()));
                request.setProjection(mapView.getOptions().getBaseProjection());
                VectorTileSearchService service = new VectorTileSearchService(mainMapLayer.getDataSource(), mainMapLayer.getTileDecoder());
                service.setMaxZoom(14);
                service.setMinZoom(14);
                StringVector layers = new StringVector();
                layers.add("poi");
                layers.add("transportation_name");
                layers.add("place");
                service.setLayers(layers);
                service.setSortByDistance(true);
                service.setPreventDuplicates(true);
                VectorTileFeatureCollection result = service.findFeatures(request);
                Log.d("TAG", "testVectorTileSearch done " + result.getFeatureCount() + " " + ((System.nanoTime() - startTime)/1000000));
                GeoJSONGeometryWriter writer = new GeoJSONGeometryWriter();
                ;
                largeLog("TAG", writer.writeFeatureCollection(result));
            }
        });
        thread.start();
    }


   public void testValhallaBicycle(String dataPath, Options options) {
       Projection projection = options.getBaseProjection();
       MultiValhallaOfflineRoutingService routingService = new MultiValhallaOfflineRoutingService();
       routingService.add(dataPath+"/france/france.vtiles");
       routingService.addLocale("fr-FR", "{\"posix_locale\":\"fr_FR.UTF-8\",\"aliases\":[\"fr\"],\"instructions\":{\"arrive\":{\"phrases\":{\"0\":\"Arriver : <TIME>.\",\"1\":\"Arriver : <TIME> \\u00E0 <TRANSIT_STOP>.\"},\"example_phrases\":{\"0\":[\"Arriver : 8:02 AM.\"],\"1\":[\"Arriver : 8:02 AM \\u00E0 8 St - NYU.\"]}},\"arrive_verbal\":{\"phrases\":{\"0\":\"Arriver \\u00E0 <TIME>.\",\"1\":\"Arriver \\u00E0 <TIME> \\u00E0 <TRANSIT_STOP>.\"},\"example_phrases\":{\"0\":[\"Arriver \\u00E0 8:02 AM.\"],\"1\":[\"Arriver \\u00E0 8:02 AM \\u00E0 8 St - NYU.\"]}},\"bear\":{\"phrases\":{\"0\":\"Serrez \\u00E0 <RELATIVE_DIRECTION>.\",\"1\":\"Serrez \\u00E0 <RELATIVE_DIRECTION> dans <STREET_NAMES>.\",\"2\":\"Serrez \\u00E0 <RELATIVE_DIRECTION> dans <BEGIN_STREET_NAMES>. Continuez sur <STREET_NAMES>.\",\"3\":\"Serrez \\u00E0 <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES>.\",\"4\":\"Serrez \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <JUNCTION_NAME>.\",\"5\":\"Serrez \\u00E0 <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Serrez \\u00E0 droite.\"],\"1\":[\"Serrez \\u00E0 gauche dans Arlen Road.\"],\"2\":[\"Serrez \\u00E0 droite dans Belair Road\\/US 1 Business. Continuez sur US 1 Business.\"],\"3\":[\"Serrez \\u00E0 gauche pour rester sur US 15 South.\"],\"4\":[\"Bear right at Mannenbashi East.\"],\"5\":[\"Bear left toward Baltimore.\"]}},\"bear_verbal\":{\"phrases\":{\"0\":\"Serrez \\u00E0 <RELATIVE_DIRECTION>.\",\"1\":\"Serrez \\u00E0 <RELATIVE_DIRECTION> dans <STREET_NAMES>.\",\"2\":\"Serrez \\u00E0 <RELATIVE_DIRECTION> dans <BEGIN_STREET_NAMES>.\",\"3\":\"Serrez \\u00E0 <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES>.\",\"4\":\"Serrez \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <JUNCTION_NAME>.\",\"5\":\"Serrez \\u00E0 <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Serrez \\u00E0 droite.\"],\"1\":[\"Serrez \\u00E0 gauche dans Arlen Road.\"],\"2\":[\"Serrez \\u00E0 droite dans Belair Road, U.S. 1 Business.\"],\"3\":[\"Serrez \\u00E0 gauche pour rester sur U.S. 15 South.\"],\"4\":[\"Bear right at Mannenbashi East.\"],\"5\":[\"Bear left toward Baltimore.\"]}},\"becomes\":{\"phrases\":{\"0\":\"<PREVIOUS_STREET_NAMES> devient <STREET_NAMES>.\"},\"example_phrases\":{\"0\":[\"Vine Street devient Middletown Road.\"]}},\"becomes_verbal\":{\"phrases\":{\"0\":\"<PREVIOUS_STREET_NAMES> devient <STREET_NAMES>.\"},\"example_phrases\":{\"0\":[\"Vine Street devient Middletown Road.\"]}},\"continue\":{\"phrases\":{\"0\":\"Continuez.\",\"1\":\"Continuez sur <STREET_NAMES>.\",\"2\":\"Continuez \\u00E0 <JUNCTION_NAME>.\",\"3\":\"Continuez vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"example_phrases\":{\"0\":[\"Continuez.\"],\"1\":[\"Continuez sur 10th Avenue.\"],\"2\":[\"Continue at Mannenbashi East.\"],\"3\":[\"Continue toward Baltimore.\"]}},\"continue_verbal\":{\"phrases\":{\"0\":\"Continuez.\",\"1\":\"Continuez pendant <LENGTH>.\",\"2\":\"Continuez sur <STREET_NAMES>.\",\"3\":\"Continuez vers <STREET_NAMES> pendant <LENGTH>.\",\"4\":\"Continuez \\u00E0 <JUNCTION_NAME>.\",\"5\":\"Continuez \\u00E0 <JUNCTION_NAME> pendant <LENGTH>.\",\"6\":\"Continuez vers <TOWARD_SIGN>.\",\"7\":\"Continuez vers <TOWARD_SIGN> pendant <LENGTH>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"metric_lengths\":[\"<KILOMETERS> kilom\\u00E8tres\",\"1 kilometre\",\"<METERS> m\\u00E8tres\",\"moins de 10 m\\u00E8tres\"],\"us_customary_lengths\":[\"<MILES> miles\",\"1 mile\",\"un demi mile\",\"un quart de mile\",\"<FEET> pieds\",\"moins de 10 pieds\"],\"example_phrases\":{\"0\":[\"Continue.\"],\"1\":[\"Continue for 300 feet.\"],\"2\":[\"Continue on 10th Avenue.\"],\"3\":[\"Continue on 10th Avenue for 3 miles.\"],\"4\":[\"Continue at Mannenbashi East.\"],\"5\":[\"Continue at Mannenbashi East for 2 miles.\"],\"6\":[\"Continue toward Baltimore.\"],\"7\":[\"Continue toward Baltimore for 5 miles.\"]}},\"continue_verbal_alert\":{\"phrases\":{\"0\":\"Continuez.\",\"1\":\"Continuez sur <STREET_NAMES>.\",\"2\":\"Continuez \\u00E0 <JUNCTION_NAME>.\",\"3\":\"Continuez vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"example_phrases\":{\"0\":[\"Continuez.\"],\"1\":[\"Continuez sur 10th Avenue.\"],\"2\":[\"Continue at Mannenbashi East.\"],\"3\":[\"Continue toward Baltimore.\"]}},\"depart\":{\"phrases\":{\"0\":\"D\\u00E9part : <TIME>.\",\"1\":\"D\\u00E9part : <TIME> de <TRANSIT_STOP>.\"},\"example_phrases\":{\"0\":[\"D\\u00E9part : 8:02 AM.\"],\"1\":[\"D\\u00E9part : 8:02 AM de 8 St - NYU.\"]}},\"depart_verbal\":{\"phrases\":{\"0\":\"D\\u00E9part \\u00E0 <TIME>.\",\"1\":\"D\\u00E9part \\u00E0 <TIME> de <TRANSIT_STOP>.\"},\"example_phrases\":{\"0\":[\"D\\u00E9part at 8:02 AM.\"],\"1\":[\"D\\u00E9part at 8:02 AM from 8 St - NYU.\"]}},\"destination\":{\"phrases\":{\"0\":\"Vous \\u00EAtes arriv\\u00E9 \\u00E0 votre destination.\",\"1\":\"Vous \\u00EAtes arriv\\u00E9 \\u00E0 <DESTINATION>.\",\"2\":\"Votre destination est sur la <RELATIVE_DIRECTION>.\",\"3\":\"<DESTINATION> est sur la <RELATIVE_DIRECTION>.\"},\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Vous \\u00EAtes arriv\\u00E9 \\u00E0 votre destination.\"],\"1\":[\"Vous \\u00EAtes arriv\\u00E9 \\u00E0 3206 Powelton Avenue.\"],\"2\":[\"Votre destination est sur la gauche.\",\"Votre destination est sur la droite.\"],\"3\":[\"Lancaster Brewing Company est sur la gauche.\"]}},\"destination_verbal\":{\"phrases\":{\"0\":\"Vous \\u00EAtes arriv\\u00E9 \\u00E0 votre destination.\",\"1\":\"Vous \\u00EAtes arriv\\u00E9 \\u00E0 <DESTINATION>.\",\"2\":\"Votre destination est sur la <RELATIVE_DIRECTION>.\",\"3\":\"<DESTINATION> est sur la <RELATIVE_DIRECTION>.\"},\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Vous \\u00EAtes arriv\\u00E9 \\u00E0 votre destination.\"],\"1\":[\"Vous \\u00EAtes arriv\\u00E9 \\u00E0 32 o6 Powelton Avenue.\"],\"2\":[\"Votre destination est sur la gauche.\",\"Votre destination est sur la droite.\"],\"3\":[\"Lancaster Brewing Company est sur la gauche.\"]}},\"destination_verbal_alert\":{\"phrases\":{\"0\":\"Vous arriverez \\u00E0 votre destination.\",\"1\":\"Vous arriverez \\u00E0 <DESTINATION>.\",\"2\":\"Votre destination sera sur la <RELATIVE_DIRECTION>.\",\"3\":\"<DESTINATION> sera sur la <RELATIVE_DIRECTION>.\"},\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Vous arriverez \\u00E0 votre destination.\"],\"1\":[\"Vous arriverez \\u00E0 32 o6 Powelton Avenue.\"],\"2\":[\"Votre destination sera sur la gauche.\",\"Votre destination sera sur la droite.\"],\"3\":[\"Lancaster Brewing Company sera sur la gauche.\"]}},\"enter_ferry\":{\"phrases\":{\"0\":\"Prenez le ferry.\",\"1\":\"Prenez <STREET_NAMES>.\",\"2\":\"Prenez <STREET_NAMES> <FERRY_LABEL>.\",\"3\":\"Prenez le ferry vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"ferry_label\":\"Ferry\",\"example_phrases\":{\"0\":[\"Prenez le ferry.\"],\"1\":[\"Prenez Millersburg Ferry.\"],\"2\":[\"Prenez Bridgeport - Port Jefferson Ferry.\"],\"3\":[\"Take the ferry toward Cape May.\"]}},\"enter_ferry_verbal\":{\"phrases\":{\"0\":\"Prenez le ferry.\",\"1\":\"Prenez <STREET_NAMES>.\",\"2\":\"Prenez <STREET_NAMES> <FERRY_LABEL>.\",\"3\":\"Prenez le ferry vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"ferry_label\":\"Ferry\",\"example_phrases\":{\"0\":[\"Prenez le ferry.\"],\"1\":[\"Prenez Millersburg Ferry.\"],\"2\":[\"Prenez Bridgeport - Port Jefferson Ferry.\"],\"3\":[\"Take the ferry toward Cape May.\"]}},\"enter_roundabout\":{\"phrases\":{\"0\":\"Entrez dans le rond-point.\",\"1\":\"Entrez dans le rond-point et prenez la <ORDINAL_VALUE> sortie.\",\"2\":\"Entrez dans le rond-point et prenez la <ORDINAL_VALUE> sortie dans <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"3\":\"Entrez dans le rond-point et prenez la <ORDINAL_VALUE> sortie dans <ROUNDABOUT_EXIT_BEGIN_STREET_NAMES>. Continuez sur <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"4\":\"Entrez dans le rond-point et prenez la <ORDINAL_VALUE> sortie vers <TOWARD_SIGN>.\",\"5\":\"Entrez dans le rond-point et prenez la sortie dans <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"6\":\"Entrez dans le rond-point et prenez la sortie dans <ROUNDABOUT_EXIT_BEGIN_STREET_NAMES>. Continuez sur <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"7\":\"Entrez sur le rond-point et prenez la sortie vers <TOWARD_SIGN>.\",\"8\":\"Entrez dans <STREET_NAMES>.\",\"9\":\"Entrez dans <STREET_NAMES> et prenez la <ORDINAL_VALUE> sortie.\",\"10\":\"Entrez dans <STREET_NAMES> et prenez la <ORDINAL_VALUE> sortie dans <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"11\":\"Entrez dans <STREET_NAMES> et prenez la <ORDINAL_VALUE> sortie dans <ROUNDABOUT_EXIT_BEGIN_STREET_NAMES>. Continuez sur <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"12\":\"Entrez dans <STREET_NAMES> et prenez la <ORDINAL_VALUE> sortie vers <TOWARD_SIGN>.\",\"13\":\"Entrez dans <STREET_NAMES> et prenez la sortie dans <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"14\":\"Entrez dans <STREET_NAMES> et prenez la sortie dans <ROUNDABOUT_EXIT_BEGIN_STREET_NAMES>. Continuez sur <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"15\":\"Entrez dans <STREET_NAMES> et prenez la sortie vers <TOWARD_SIGN>.\"},\"ordinal_values\":[\"1er\",\"2e\",\"3\\u00E8me\",\"4\\u00E8me\",\"5\\u00E8me\",\"6\\u00E8me\",\"7\\u00E8me\",\"8\\u00E8me\",\"9\\u00E8me\",\"10\\u00E8me\"],\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"example_phrases\":{\"0\":[\"Entrez dans le rond-point.\"],\"1\":[\"Entrez dans le rond-point et prenez la 1er sortie.\",\"Entrez dans le rond-point et prenez la 2nd sortie.\",\"Entrez dans le rond-point et prenez la 3\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 4\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 5\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 6\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 7\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 8\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 9\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 10\\u00E8me sortie.\"],\"2\":[\"Enter the roundabout and take the 3rd exit onto Main Street.\"],\"3\":[\"Enter the roundabout and take the 3rd exit onto US 322\\/Main Street. Continue on US 322.\"],\"4\":[\"Enter the roundabout and take the 3rd exit toward Baltimore.\"],\"5\":[\"Enter the roundabout and take the exit onto Main Street.\"],\"6\":[\"Enter the roundabout and take the exit onto US 322\\/Main Street. Continue on US 322.\"],\"7\":[\"Enter the roundabout and take the exit toward Baltimore.\"],\"8\":[\"Enter Dupont Circle.\"],\"9\":[\"Enter Dupont Circle and take the 1st exit.\"],\"10\":[\"Enter Dupont Circle and take the 3rd exit onto Main Street.\"],\"11\":[\"Enter Dupont Circle and take the 3rd exit onto US 322\\/Main Street. Continue on US 322.\"],\"12\":[\"Enter Dupont Circle and take the 3rd exit toward Baltimore.\"],\"13\":[\"Enter Dupont Circle and take the exit onto Main Street.\"],\"14\":[\"Enter Dupont Circle and take the exit onto US 322\\/Main Street. Continue on US 322.\"],\"15\":[\"Enter Dupont Circle and take the exit toward Baltimore.\"]}},\"enter_roundabout_verbal\":{\"phrases\":{\"0\":\"Entrez dans le rond-point.\",\"1\":\"Entrez dans le rond-point et prenez la <ORDINAL_VALUE> sortie.\",\"2\":\"Entrez dans le rond-point et prenez la <ORDINAL_VALUE> sortie dans <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"3\":\"Entrez dans le rond-point et prenez la <ORDINAL_VALUE> sortie dans <ROUNDABOUT_EXIT_BEGIN_STREET_NAMES>.\",\"4\":\"Entrez dans le rond-point et prenez la <ORDINAL_VALUE> sortie vers <TOWARD_SIGN>.\",\"5\":\"Entrez dans le rond-point et prenez la sortie dans <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"6\":\"Entrez dans le rond-point et prenez la sortie dans <ROUNDABOUT_EXIT_BEGIN_STREET_NAMES>.\",\"7\":\"Entrez sur le rond-point et prenez la sortie vers <TOWARD_SIGN>.\",\"8\":\"Entrez dans <STREET_NAMES>.\",\"9\":\"Entrez dans <STREET_NAMES> et prenez la <ORDINAL_VALUE> sortie.\",\"10\":\"Entrez dans <STREET_NAMES> et prenez la <ORDINAL_VALUE> sortie dans <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"11\":\"Entrez dans <STREET_NAMES> et prenez la <ORDINAL_VALUE> sortie dans <ROUNDABOUT_EXIT_BEGIN_STREET_NAMES>.\",\"12\":\"Entrez dans <STREET_NAMES> et prenez la <ORDINAL_VALUE> sortie vers <TOWARD_SIGN>.\",\"13\":\"Entrez dans <STREET_NAMES> et prenez la sortie dans <ROUNDABOUT_EXIT_STREET_NAMES>.\",\"14\":\"Entrez dans <STREET_NAMES> et prenez la sortie dans <ROUNDABOUT_EXIT_BEGIN_STREET_NAMES>.\",\"15\":\"Entrez dans <STREET_NAMES> et prenez la sortie vers <TOWARD_SIGN>.\"},\"ordinal_values\":[\"1er\",\"2e\",\"3\\u00E8me\",\"4\\u00E8me\",\"5\\u00E8me\",\"6\\u00E8me\",\"7\\u00E8me\",\"8\\u00E8me\",\"9\\u00E8me\",\"10\\u00E8me\"],\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"example_phrases\":{\"0\":[\"Entrez dans le rond-point.\"],\"1\":[\"Entrez dans le rond-point et prenez la 1st sortie.\",\"Entrez dans le rond-point et prenez la 2nd sortie.\",\"Entrez dans le rond-point et prenez la 3\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 4\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 5\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 6\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 7\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 8\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 9\\u00E8me sortie.\",\"Entrez dans le rond-point et prenez la 10\\u00E8me sortie.\"],\"2\":[\"Enter the roundabout and take the 3rd exit onto Main Street.\"],\"3\":[\"Enter the roundabout and take the 3rd exit onto U.S. 3 22\\/Main Street.\"],\"4\":[\"Enter the roundabout and take the 3rd exit toward Baltimore.\"],\"5\":[\"Enter the roundabout and take the exit onto Main Street.\"],\"6\":[\"Enter the roundabout and take the exit onto U.S. 3 22\\/Main Street.\"],\"7\":[\"Enter the roundabout and take the exit toward Baltimore.\"],\"8\":[\"Enter Dupont Circle.\"],\"9\":[\"Enter Dupont Circle and take the 1st exit.\"],\"10\":[\"Enter Dupont Circle and take the 3rd exit onto Main Street.\"],\"11\":[\"Enter Dupont Circle and take the 3rd exit onto U.S. 3 22\\/Main Street.\"],\"12\":[\"Enter Dupont Circle and take the 3rd exit toward Baltimore.\"],\"13\":[\"Enter Dupont Circle and take the exit onto Main Street.\"],\"14\":[\"Enter Dupont Circle and take the exit onto U.S. 3 22\\/Main Street.\"],\"15\":[\"Enter Dupont Circle and take the exit toward Baltimore.\"]}},\"exit\":{\"phrases\":{\"0\":\"Prenez la sortie sur la <RELATIVE_DIRECTION>.\",\"1\":\"Prenez la sortie <NUMBER_SIGN> sur la <RELATIVE_DIRECTION>.\",\"2\":\"Prenez la sortie <BRANCH_SIGN> sur la <RELATIVE_DIRECTION>.\",\"3\":\"Prenez la sortie <NUMBER_SIGN> sur la <RELATIVE_DIRECTION> dans <BRANCH_SIGN>.\",\"4\":\"Prenez la sortie sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"5\":\"Prenez la sortie <NUMBER_SIGN> sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"6\":\"Prenez la sortie <BRANCH_SIGN> sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"7\":\"Prenez la sortie <NUMBER_SIGN> sur la <RELATIVE_DIRECTION> dans <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"8\":\"Prenez la sortie <NAME_SIGN> sur la <RELATIVE_DIRECTION>.\",\"10\":\"Prenez la sortie <NAME_SIGN> sur la <RELATIVE_DIRECTION> dans <BRANCH_SIGN>.\",\"12\":\"Prenez la sortie <NAME_SIGN> sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"14\":\"Prenez la sortie <NAME_SIGN> sur la <RELATIVE_DIRECTION> dans <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"15\":\"Prenez la sortie.\",\"16\":\"Prenez la sortie <NUMBER_SIGN>.\",\"17\":\"Prenez la sortie <BRANCH_SIGN>.\",\"18\":\"Prenez la sortie <NUMBER_SIGN> dans <BRANCH_SIGN>.\",\"19\":\"Prenez la sortie vers <TOWARD_SIGN>.\",\"20\":\"Prenez la sortie <NUMBER_SIGN> vers <TOWARD_SIGN>.\",\"21\":\"Prenez la sortie <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"22\":\"Prenez la sortie <NUMBER_SIGN> dans <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"23\":\"Prenez la sortie <NAME_SIGN>.\",\"25\":\"Prenez la sortie <NAME_SIGN> dans <BRANCH_SIGN>.\",\"27\":\"Prenez la sortie <NAME_SIGN> vers <TOWARD_SIGN>.\",\"29\":\"Prenez la sortie <NAME_SIGN> dans <BRANCH_SIGN> vers <TOWARD_SIGN>.\"},\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Prenez la sortie sur la gauche.\",\"Prenez la sortie sur la droite.\"],\"1\":[\"Prenez la sortie 67 B-A sur la droite.\"],\"2\":[\"Prenez la sortie US 322 West sur la droite.\"],\"3\":[\"Prenez la sortie 67 B-A sur la droite dans US 322 West.\"],\"4\":[\"Prenez la sortie sur la droite vers Lewistown.\"],\"5\":[\"Prenez la sortie 67 B-A sur la droite vers Lewistown.\"],\"6\":[\"Prenez la sortie US 322 West sur la droite vers Lewistown.\"],\"7\":[\"Prenez la sortie 67 B-A sur la droite dans US 322 West vers Lewistown\\/State College.\"],\"8\":[\"Prenez la sortie White Marsh Boulevard sur la gauche.\"],\"10\":[\"Prenez la sortie White Marsh Boulevard sur la gauche dans MD 43 East.\"],\"12\":[\"Prenez la sortie White Marsh Boulevard sur la gauche vers White Marsh.\"],\"14\":[\"Prenez la sortie White Marsh Boulevard sur la gauche dans MD 43 East vers White Marsh.\"],\"15\":[\"Prenez la sortie.\"],\"16\":[\"Prenez la sortie 67 B-A.\"],\"17\":[\"Prenez la sortie US 322 West.\"],\"18\":[\"Prenez la sortie 67 B-A dans US 322 West.\"],\"19\":[\"Prenez la sortie toward Lewistown.\"],\"20\":[\"Prenez la sortie 67 B-A toward Lewistown.\"],\"21\":[\"Prenez la sortie US 322 West toward Lewistown.\"],\"22\":[\"Prenez la sortie 67 B-A dans US 322 West toward Lewistown\\/State College.\"],\"23\":[\"Prenez la sortie White Marsh Boulevard.\"],\"25\":[\"Prenez la sortie White Marsh Boulevard dans MD 43 East.\"],\"27\":[\"Prenez la sortie White Marsh Boulevard toward White Marsh.\"],\"29\":[\"Prenez la sortie White Marsh Boulevard dans MD 43 East toward White Marsh.\"]}},\"exit_roundabout\":{\"phrases\":{\"0\":\"Quittez le rond-point.\",\"1\":\"Quittez le rond-point dans <STREET_NAMES>.\",\"2\":\"Quittez le rond-point dans <BEGIN_STREET_NAMES>. Continuez sur <STREET_NAMES>.\",\"3\":\"Quittez le rond-point vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"example_phrases\":{\"0\":[\"Quittez le rond-point.\"],\"1\":[\"Quittez le rond-point dans Philadelphia Road\\/MD 7.\"],\"2\":[\"Quittez le rond-point dans Catoctin Mountain Highway\\/US 15. Continuez sur US 15.\"],\"3\":[\"Exit the roundabout toward Baltimore.\"]}},\"exit_roundabout_verbal\":{\"phrases\":{\"0\":\"Quittez le rond-point.\",\"1\":\"Quittez le rond-point dans <STREET_NAMES>.\",\"2\":\"Quittez le rond-point pour <BEGIN_STREET_NAMES>.\",\"3\":\"Quittez le rond-point vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"example_phrases\":{\"0\":[\"Quittez le rond-point.\"],\"1\":[\"Quittez le rond-point pour Philadelphia Road, Maryland 7.\"],\"2\":[\"Quittez le rond-point pour Catoctin Mountain Highway, U.S. 15.\"],\"3\":[\"Exit the roundabout toward Baltimore.\"]}},\"exit_verbal\":{\"phrases\":{\"0\":\"Prenez la sortie sur la <RELATIVE_DIRECTION>.\",\"1\":\"Prenez la sortie <NUMBER_SIGN> sur la <RELATIVE_DIRECTION>.\",\"2\":\"Prenez la sortie <BRANCH_SIGN> sur la <RELATIVE_DIRECTION>.\",\"3\":\"Prenez la sortie <NUMBER_SIGN> sur la <RELATIVE_DIRECTION> dans <BRANCH_SIGN>.\",\"4\":\"Prenez la sortie sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"5\":\"Prenez la sortie <NUMBER_SIGN> sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"6\":\"Prenez la sortie <BRANCH_SIGN> sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"7\":\"Prenez la sortie <NUMBER_SIGN> sur la <RELATIVE_DIRECTION> dans <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"8\":\"Prenez la sortie <NAME_SIGN> sur la <RELATIVE_DIRECTION>.\",\"10\":\"Prenez la sortie <NAME_SIGN> sur la <RELATIVE_DIRECTION> dans <BRANCH_SIGN>.\",\"12\":\"Prenez la sortie <NAME_SIGN> sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"14\":\"Prenez la sortie <NAME_SIGN> sur la <RELATIVE_DIRECTION> dans <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"15\":\"Prenez la sortie.\",\"16\":\"Prenez la sortie <NUMBER_SIGN>.\",\"17\":\"Prenez la sortie <BRANCH_SIGN>.\",\"18\":\"Prenez la sortie <NUMBER_SIGN> dans <BRANCH_SIGN>.\",\"19\":\"Prenez la sortie vers <TOWARD_SIGN>.\",\"20\":\"Prenez la sortie <NUMBER_SIGN> vers <TOWARD_SIGN>.\",\"21\":\"Prenez la sortie <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"22\":\"Prenez la sortie <NUMBER_SIGN> dans <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"23\":\"Prenez la sortie <NAME_SIGN>.\",\"25\":\"Prenez la sortie <NAME_SIGN> dans <BRANCH_SIGN>.\",\"27\":\"Prenez la sortie <NAME_SIGN> vers <TOWARD_SIGN>.\",\"29\":\"Prenez la sortie <NAME_SIGN> dans <BRANCH_SIGN> vers <TOWARD_SIGN>.\"},\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Prenez la sortie sur la gauche.\",\"Prenez la sortie sur la droite.\"],\"1\":[\"Prenez la sortie 67 B-A sur la droite.\"],\"2\":[\"Prenez la sortie U.S. 3 22 West sur la droite.\"],\"3\":[\"Prenez la sortie 67 B-A sur la droite dans U.S. 3 22 West.\"],\"4\":[\"Prenez la sortie sur la droite vers Lewistown.\"],\"5\":[\"Prenez la sortie 67 B-A sur la droite vers Lewistown.\"],\"6\":[\"Prenez la sortie U.S. 3 22 West sur la droite vers Lewistown.\"],\"7\":[\"Prenez la sortie 67 B-A sur la droite dans U.S. 3 22 West vers Lewistown, State College.\"],\"8\":[\"Prenez la sortie White Marsh Boulevard sur la gauche.\"],\"10\":[\"Prenez la sortie White Marsh Boulevard sur la gauche dans Maryland 43 East.\"],\"12\":[\"Prenez la sortie White Marsh Boulevard sur la gauche vers White Marsh.\"],\"14\":[\"Prenez la sortie White Marsh Boulevard sur la gauche dans Maryland 43 East vers White Marsh.\"],\"15\":[\"Prenez la sortie.\"],\"16\":[\"Prenez la sortie 67 B-A.\"],\"17\":[\"Prenez la sortie US 322 West.\"],\"18\":[\"Prenez la sortie 67 B-A dans US 322 West.\"],\"19\":[\"Prenez la sortie vers Lewistown.\"],\"20\":[\"Prenez la sortie 67 B-A vers Lewistown.\"],\"21\":[\"Prenez la sortie US 322 West vers Lewistown.\"],\"22\":[\"Prenez la sortie 67 B-A dans US 322 West vers Lewistown\\/State College.\"],\"23\":[\"Prenez la sortie White Marsh Boulevard.\"],\"25\":[\"Prenez la sortie White Marsh Boulevard dans MD 43 East.\"],\"27\":[\"Prenez la sortie White Marsh Boulevard vers White Marsh.\"],\"29\":[\"Prenez la sortie White Marsh Boulevard dans MD 43 East vers White Marsh.\"]}},\"exit_visual\":{\"phrases\":{\"0\":\"Sortie n\\u00B0<EXIT_NUMBERS>\"},\"example_phrases\":{\"0\":[\"Sortie n\\u00B01A\"]}},\"keep\":{\"phrases\":{\"0\":\"Gardez la <RELATIVE_DIRECTION> \\u00E0 la fourche.\",\"1\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN>.\",\"2\":\"Gardez la <RELATIVE_DIRECTION> pour prendre <STREET_NAMES>.\",\"3\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN> dans <STREET_NAMES>.\",\"4\":\"Gardez la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"5\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN> vers <TOWARD_SIGN>.\",\"6\":\"Gardez la <RELATIVE_DIRECTION> pour prendre <STREET_NAMES> vers <TOWARD_SIGN>.\",\"7\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN> dans <STREET_NAMES> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"route\",\"droite\"],\"example_phrases\":{\"0\":[\"Gardez la gauche \\u00E0 la fourche.\",\"Gardez la route \\u00E0 la fourche.\",\"Gardez la droite \\u00E0 la fourche.\"],\"1\":[\"Gardez la droite pour prendre la sortie 62.\"],\"2\":[\"Gardez la droite pour prendre I 895 South.\"],\"3\":[\"Gardez la droite pour prendre la sortie 62 dans I 895 South.\"],\"4\":[\"Gardez la droite vers Annapolis.\"],\"5\":[\"Gardez la droite pour prendre la sortie 62 vers Annapolis.\"],\"6\":[\"Gardez la droite pour prendre I 895 South vers Annapolis.\"],\"7\":[\"Gardez la droite pour prendre la sortie 62 dans I 895 South vers Annapolis.\"]}},\"keep_to_stay_on\":{\"phrases\":{\"0\":\"Gardez la <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES>.\",\"1\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN> pour rester sur <STREET_NAMES>.\",\"2\":\"Gardez la <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES> vers <TOWARD_SIGN>.\",\"3\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN> pour rester sur <STREET_NAMES> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"route\",\"droite\"],\"example_phrases\":{\"0\":[\"Gardez la gauche pour rester sur I 95 South.\",\"Gardez la route pour rester sur I 95 South.\",\"Gardez la droite pour rester sur I 95 South.\"],\"1\":[\"Gardez la gauche pour prendre la sortie 62 pour rester sur I 95 South.\"],\"2\":[\"Gardez la gauche pour rester sur I 95 South vers Baltimore.\"],\"3\":[\"Gardez la gauche pour prendre la sortie 62 pour rester sur I 95 South vers Baltimore.\"]}},\"keep_to_stay_on_verbal\":{\"phrases\":{\"0\":\"Gardez la <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES>.\",\"1\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN> pour rester sur <STREET_NAMES>.\",\"2\":\"Gardez la <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES> vers <TOWARD_SIGN>.\",\"3\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN> pour rester sur <STREET_NAMES> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"route\",\"droite\"],\"example_phrases\":{\"0\":[\"Gardez la gauche pour rester sur Interstate 95 South.\",\"Gardez la route pour rester sur Interstate 95 South.\",\"Gardez la droite pour rester sur Interstate 95 South.\"],\"1\":[\"Gardez la gauche pour prendre la sortie 62 pour rester sur Interstate 95 South.\"],\"2\":[\"Gardez la gauche pour rester sur I 95 South vers Baltimore.\"],\"3\":[\"Gardez la gauche pour prendre la sortie 62 pour rester sur Interstate 95 South vers Baltimore.\"]}},\"keep_verbal\":{\"phrases\":{\"0\":\"Gardez la <RELATIVE_DIRECTION> \\u00E0 la fourche.\",\"1\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN>.\",\"2\":\"Gardez la <RELATIVE_DIRECTION> pour prendre <STREET_NAMES>.\",\"3\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN> dans <STREET_NAMES>.\",\"4\":\"Gardez la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"5\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN> vers <TOWARD_SIGN>.\",\"6\":\"Gardez la <RELATIVE_DIRECTION> pour prendre <STREET_NAMES> vers <TOWARD_SIGN>.\",\"7\":\"Gardez la <RELATIVE_DIRECTION> pour prendre la sortie <NUMBER_SIGN> dans <STREET_NAMES> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"route\",\"droite\"],\"example_phrases\":{\"0\":[\"Gardez la gauche \\u00E0 la fourche.\",\"Gardez la route \\u00E0 la fourche.\",\"Gardez la droite \\u00E0 la fourche.\"],\"1\":[\"Gardez la droite pour prendre la sortie 62.\"],\"2\":[\"Gardez la droite pour prendre Interstate 8 95 South.\"],\"3\":[\"Gardez la droite pour prendre la sortie 62 dans Interstate 8 95 South.\"],\"4\":[\"Gardez la droite vers Annapolis.\"],\"5\":[\"Gardez la droite pour prendre la sortie 62 vers Annapolis.\"],\"6\":[\"Gardez la droite pour prendre Interstate 8 95 South vers Annapolis.\"],\"7\":[\"Gardez la droite pour prendre la sortie 62 dans Interstate 8 95 South vers Annapolis.\"]}},\"merge\":{\"phrases\":{\"0\":\"Ins\\u00E9rez-vous.\",\"1\":\"Ins\\u00E9rez-vous \\u00E0 <RELATIVE_DIRECTION>.\",\"2\":\"Ins\\u00E9rez-vous sur <STREET_NAMES>.\",\"3\":\"Ins\\u00E9rez-vous \\u00E0 <RELATIVE_DIRECTION> sur <STREET_NAMES>.\",\"4\":\"Ins\\u00E9rez-vous vers <TOWARD_SIGN>.\",\"5\":\"Ins\\u00E9rez-vous \\u00E0 <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\"},\"relative_directions\":[\"gauche\",\"droite\"],\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"example_phrases\":{\"0\":[\"Rejoignez.\"],\"1\":[\"Rejoignez \\u00E0 gauche.\"],\"2\":[\"Rejoignez I 76 West\\/Pennsylvania Turnpike.\"],\"3\":[\"Rejoignez \\u00E0 droite I 83 South.\"],\"4\":[\"Merge toward Baltimore.\"],\"5\":[\"Merge right toward Baltimore.\"]}},\"merge_verbal\":{\"phrases\":{\"0\":\"Ins\\u00E9rez-vous.\",\"1\":\"Ins\\u00E9rez-vous \\u00E0 <RELATIVE_DIRECTION>.\",\"2\":\"Ins\\u00E9rez-vous sur <STREET_NAMES>.\",\"3\":\"Ins\\u00E9rez-vous \\u00E0 <RELATIVE_DIRECTION> sur <STREET_NAMES>.\",\"4\":\"Ins\\u00E9rez-vous vers <TOWARD_SIGN>.\",\"5\":\"Ins\\u00E9rez-vous \\u00E0 <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\"},\"relative_directions\":[\"gauche\",\"droite\"],\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"example_phrases\":{\"0\":[\"Merge.\"],\"1\":[\"Rejoignez \\u00E0 gauche.\"],\"2\":[\"Rejoignez I 76 West\\/Pennsylvania Turnpike.\"],\"3\":[\"Rejoignez \\u00E0 droite I 83 South.\"],\"4\":[\"Merge toward Baltimore.\"],\"5\":[\"Merge right toward Baltimore.\"]}},\"post_transition_transit_verbal\":{\"phrases\":{\"0\":\"Voyage \\u00E0 <TRANSIT_STOP_COUNT> <TRANSIT_STOP_COUNT_LABEL>.\"},\"transit_stop_count_labels\":{\"one\":\"arr\\u00EAt\",\"other\":\"arr\\u00EAts\"},\"example_phrases\":{\"0\":[\"Travel 1 stop.\",\"Travel 3 stops.\"]}},\"post_transition_verbal\":{\"phrases\":{\"0\":\"Continuez pendant <LENGTH>.\",\"1\":\"Continuez vers <STREET_NAMES> pendant <LENGTH>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"metric_lengths\":[\"<KILOMETERS> kilom\\u00E8tres\",\"1 kilom\\u00E8tre\",\"<METERS> m\\u00E8tres\",\"moins de 10 m\\u00E8tres\"],\"us_customary_lengths\":[\"<MILES> miles\",\"1 mile\",\"un demi mile\",\"un quart de mile\",\"<FEET> pieds\",\"moins de 10 pieds\"],\"example_phrases\":{\"0\":[\"Continuez pendant 300 pieds.\",\"Continuez pendant 9 miles.\"],\"1\":[\"Continuez sur Pennsylvania 7 43 pendant 6.2 miles.\",\"Continuez sur Main Street, Vermont 30 pendant 1 dixi\\u00E8me de mile.\"]}},\"ramp\":{\"phrases\":{\"0\":\"Prenez la bretelle sur la <RELATIVE_DIRECTION>.\",\"1\":\"Prenez la bretelle <BRANCH_SIGN> sur la <RELATIVE_DIRECTION>.\",\"2\":\"Prenez la bretelle sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"3\":\"Prenez la bretelle <BRANCH_SIGN> sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"4\":\"Prenez la bretelle <NAME_SIGN> sur la <RELATIVE_DIRECTION>.\",\"5\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour prendre la bretelle.\",\"6\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour prendre la bretelle <BRANCH_SIGN>.\",\"7\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour prendre la bretelle vers <TOWARD_SIGN>.\",\"8\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour prendre la bretelle <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"9\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour prendre la bretelle <NAME_SIGN>.\",\"10\":\"Prenez la bretelle.\",\"11\":\"Prenez la bretelle <BRANCH_SIGN>.\",\"12\":\"Prenez la bretelle vers <TOWARD_SIGN>.\",\"13\":\"Prenez la bretelle <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"14\":\"Prenez la bretelle <NAME_SIGN>.\"},\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Prenez la bretelle sur la gauche.\",\"Prenez la bretelle sur la droite.\"],\"1\":[\"Prenez la bretelle I 95 sur la droite.\"],\"2\":[\"Prenez la bretelle sur la gauche vers JFK.\"],\"3\":[\"Prenez la bretelle South Conduit Avenue sur la gauche vers JFK.\"],\"4\":[\"Prenez la bretelle Gettysburg Pike sur la droite.\"],\"5\":[\"Tournez \\u00E0 gauche pour prendre la bretelle.\",\"Tournez \\u00E0 droite pour prendre la bretelle.\"],\"6\":[\"Tournez \\u00E0 gauche pour prendre la bretelle PA 283 West.\"],\"7\":[\"Tournez \\u00E0 gauche pour prendre la bretelle vers Harrisburg\\/Harrisburg International Airport.\"],\"8\":[\"Tournez \\u00E0 gauche pour prendre la bretelle PA 283 West vers Harrisburg\\/Harrisburg International Airport.\"],\"9\":[\"Tournez \\u00E0 droite pour prendre la bretelle Gettysburg Pike.\"],\"10\":[\"Prenez la bretelle.\"],\"11\":[\"Prenez la bretelle I 95.\"],\"12\":[\"Prenez la bretelle vers JFK.\"],\"13\":[\"Prenez la bretelle Soutch Conduit Avenue vers JFK.\"],\"14\":[\"Prenez la bretelle Gettysburg.\"]}},\"ramp_straight\":{\"phrases\":{\"0\":\"Continuez tout droit pour prendre la bretelle.\",\"1\":\"Continuez tout droit pour prendre la bretelle <BRANCH_SIGN>.\",\"2\":\"Continuez tout droit pour prendre la bretelle vers <TOWARD_SIGN>.\",\"3\":\"Continuez tout droit pour prendre la bretelle <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"4\":\"Continuez tout droit pour prendre la bretelle <NAME_SIGN>.\"},\"example_phrases\":{\"0\":[\"Continuez tout droit pour prendre la bretelle.\"],\"1\":[\"Continuez tout droit pour prendre la bretelle US 322 East.\"],\"2\":[\"Continuez tout droit pour prendre la bretelle vers Hershey.\"],\"3\":[\"Continuez tout droit pour prendre la bretelle US 322 East\\/US 422 East\\/US 522 East\\/US 622 East vers Hershey\\/Palmdale\\/Palmyra\\/Campbelltown.\"],\"4\":[\"Continuez tout droit pour prendre la bretelle Gettysburg Pike.\"]}},\"ramp_straight_verbal\":{\"phrases\":{\"0\":\"Continuez tout droit pour prendre la bretelle.\",\"1\":\"Continuez tout droit pour prendre la bretelle <BRANCH_SIGN>.\",\"2\":\"Continuez tout droit pour prendre la bretelle vers <TOWARD_SIGN>.\",\"3\":\"Continuez tout droit pour prendre la bretelle <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"4\":\"Continuez tout droit pour prendre la bretelle <NAME_SIGN>.\"},\"example_phrases\":{\"0\":[\"Continuez tout droit pour prendre la bretelle.\"],\"1\":[\"Continuez tout droit pour prendre la bretelle US 322 East.\"],\"2\":[\"Continuez tout droit pour prendre la bretelle vers Hershey.\"],\"3\":[\"Continuez tout droit pour prendre la bretelle US 322 East\\/US 422 East\\/US 522 East\\/US 622 East vers Hershey\\/Palmdale\\/Palmyra\\/Campbelltown.\"],\"4\":[\"Continuez tout droit pour prendre la bretelle Gettysburg Pike.\"]}},\"ramp_verbal\":{\"phrases\":{\"0\":\"Prenez la bretelle sur la <RELATIVE_DIRECTION>.\",\"1\":\"Prenez la bretelle <BRANCH_SIGN> sur la <RELATIVE_DIRECTION>.\",\"2\":\"Prenez la bretelle sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"3\":\"Prenez la bretelle <BRANCH_SIGN> sur la <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\",\"4\":\"Prenez la bretelle <NAME_SIGN> sur la <RELATIVE_DIRECTION>.\",\"5\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour prendre la bretelle.\",\"6\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour prendre la bretelle <BRANCH_SIGN>.\",\"7\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour prendre la bretelle vers <TOWARD_SIGN>.\",\"8\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour prendre la bretelle <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"9\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour prendre la bretelle <NAME_SIGN>.\",\"10\":\"Prenez la bretelle.\",\"11\":\"Prenez la bretelle <BRANCH_SIGN>.\",\"12\":\"Prenez la bretelle vers <TOWARD_SIGN>.\",\"13\":\"Prenez la bretelle <BRANCH_SIGN> vers <TOWARD_SIGN>.\",\"14\":\"Prenez la bretelle <NAME_SIGN>.\"},\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Prenez la bretelle sur la gauche.\",\"Prenez la bretelle sur la droite.\"],\"1\":[\"Prenez la bretelle Interstate 95 sur la droite.\"],\"2\":[\"Prenez la bretelle sur la gauche vers JFK.\"],\"3\":[\"Prenez la bretelle South Conduit Avenue sur la gauche vers JFK.\"],\"4\":[\"Prenez la bretelle Gettysburg Pike sur la droite.\"],\"5\":[\"Tournez \\u00E0 gauche pour prendre la bretelle.\",\"Tournez \\u00E0 droite pour prendre la bretelle.\"],\"6\":[\"Tournez \\u00E0 gauche pour prendre la bretelle Pennsylvania 2 83 West.\"],\"7\":[\"Tournez \\u00E0 gauche pour prendre la bretelle vers Harrisburg\\/Harrisburg International Airport.\"],\"8\":[\"Tournez \\u00E0 gauche pour prendre la bretelle Pennsylvania 2 83 West vers Harrisburg, Harrisburg International Airport.\"],\"9\":[\"Tournez \\u00E0 droite pour prendre la bretelle Gettysburg Pike.\"],\"10\":[\"Prenez la bretelle.\"],\"11\":[\"Prenez la bretelle Interstate 95.\"],\"12\":[\"Prenez la bretelle vers JFK.\"],\"13\":[\"Prenez la bretelle South Conduit Avenue vers JFK.\"],\"14\":[\"Prenez la bretelle Gettysburg Pike.\"]}},\"sharp\":{\"phrases\":{\"0\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION>.\",\"1\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION> dans <STREET_NAMES>.\",\"2\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION> dans <BEGIN_STREET_NAMES>. Continuez sur <STREET_NAMES>.\",\"3\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES>.\",\"4\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <JUNCTION_NAME>.\",\"5\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Tournez \\u00E0 tout de suite \\u00E0 gauche.\"],\"1\":[\"Tournez \\u00E0 tout de suite \\u00E0 droite dans Flatbush Avenue.\"],\"2\":[\"Tournez \\u00E0 tout de suite \\u00E0 gauche dans North Bond Street\\/US 1 Business\\/MD 924. Continuez sur MD 924.\"],\"3\":[\"Tournez \\u00E0 tout de suite \\u00E0 droite pour rester sur Sunstone Drive.\"],\"4\":[\"Make a sharp right at Mannenbashi East.\"],\"5\":[\"Make a sharp left toward Baltimore.\"]}},\"sharp_verbal\":{\"phrases\":{\"0\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION>.\",\"1\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION> dans <STREET_NAMES>.\",\"2\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION> dans <BEGIN_STREET_NAMES>.\",\"3\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES>.\",\"4\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <JUNCTION_NAME>.\",\"5\":\"Tournez tout de suite \\u00E0 <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Tournez \\u00E0 tout de suite \\u00E0 gauche.\"],\"1\":[\"Tournez \\u00E0 tout de suite \\u00E0 droite dans Flatbush Avenue.\"],\"2\":[\"Tournez \\u00E0 tout de suite \\u00E0 gauche dans North Bond Street, U.S. 1 Business.\"],\"3\":[\"Tournez \\u00E0 tout de suite \\u00E0 droite pour rester sur Sunstone Drive.\"],\"4\":[\"Make a sharp right at Mannenbashi East.\"],\"5\":[\"Make a sharp left toward Baltimore.\"]}},\"start\":{\"phrases\":{\"0\":\"Allez vers <CARDINAL_DIRECTION>.\",\"1\":\"Allez vers <CARDINAL_DIRECTION> sur <STREET_NAMES>.\",\"2\":\"Allez vers <CARDINAL_DIRECTION> sur <BEGIN_STREET_NAMES>. Continuez sur <STREET_NAMES>.\",\"4\":\"Conduisez vers <CARDINAL_DIRECTION>.\",\"5\":\"Conduisez vers <CARDINAL_DIRECTION> sur <STREET_NAMES>.\",\"6\":\"Conduisez vers <CARDINAL_DIRECTION> sur <BEGIN_STREET_NAMES>. Continuez sur <STREET_NAMES>.\",\"8\":\"Marchez vers <CARDINAL_DIRECTION>.\",\"9\":\"Marchez vers <CARDINAL_DIRECTION> sur <STREET_NAMES>.\",\"10\":\"Marchez vers <CARDINAL_DIRECTION> sur <BEGIN_STREET_NAMES>. Continuez sur <STREET_NAMES>.\",\"16\":\"P\\u00E9dalez vers <CARDINAL_DIRECTION>.\",\"17\":\"P\\u00E9dalez vers <CARDINAL_DIRECTION> sur <STREET_NAMES>.\",\"18\":\"P\\u00E9dalez vers <CARDINAL_DIRECTION> sur <BEGIN_STREET_NAMES>. Continuez sur <STREET_NAMES>.\"},\"cardinal_directions\":[\"le nord\",\"le nord-est\",\"l'est\",\"le sud-est\",\"le sud\",\"le sud-est\",\"l'ouest\",\"le nord-ouest\"],\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"example_phrases\":{\"0\":[\"Allez vers l'est.\",\"Allez vers le nord.\"],\"1\":[\"Allez vers le sud-est sur la 5th Avenue.\",\"Allez vers l'ouest sur l'all\\u00E9e\",\"Allez vers l'est sur la piste cyclable\",\"Allez vers le nord sur la piste de v\\u00E9lo de montagne\"],\"2\":[\"Allez vers le sud sur North Prince Street\\/US 222\\/PA 272. Continuez sur US 222\\/PA 272.\"],\"4\":[\"Conduisez vers l'est.\",\"Conduisez vers le nord.\"],\"5\":[\"Conduisez vers le sud-est sur 5th Avenue.\"],\"6\":[\"Conduisez vers le sud sur North Prince Street\\/US 222\\/PA 272. Continuez sur US 222\\/PA 272.\"],\"8\":[\"Marchez vers l'est.\",\"Marchez vers le nord.\"],\"9\":[\"Marchez vers le sud-est sur 5th Avenue.\",\"Marchez vers l'ouest sur l'all\\u00E9e\"],\"10\":[\"Marchez vers le sud sur North Prince Street\\/US 222\\/PA 272. Continuez sur US 222\\/PA 272.\"],\"16\":[\"P\\u00E9dalez vers l'est.\",\"P\\u00E9dalez vers le nord.\"],\"17\":[\"P\\u00E9dalez vers le sud-est sur 5th Avenue.\",\"P\\u00E9dalez vers l'est sur la piste cyclable\",\"P\\u00E9dalez vers le nord sur la piste de v\\u00E9lo de montagne\"],\"18\":[\"P\\u00E9dalez vers le sud sur North Prince Street\\/US 222\\/PA 272. Continuez sur US 222\\/PA 272.\"]}},\"start_verbal\":{\"phrases\":{\"0\":\"Allez vers <CARDINAL_DIRECTION>.\",\"1\":\"Allez vers <CARDINAL_DIRECTION> pendant <LENGTH>.\",\"2\":\"Allez vers <CARDINAL_DIRECTION> sur <STREET_NAMES>.\",\"3\":\"Allez vers <CARDINAL_DIRECTION> sur <STREET_NAMES> pendant <LENGTH>.\",\"4\":\"Allez vers <CARDINAL_DIRECTION> sur <BEGIN_STREET_NAMES>.\",\"5\":\"Conduisez vers <CARDINAL_DIRECTION>.\",\"6\":\"Conduisez vers <CARDINAL_DIRECTION> pendant <LENGTH>.\",\"7\":\"Conduisez vers <CARDINAL_DIRECTION> sur <STREET_NAMES>.\",\"8\":\"Conduisez vers <CARDINAL_DIRECTION> sur <STREET_NAMES> pendant <LENGTH>.\",\"9\":\"Conduisez vers <CARDINAL_DIRECTION> sur <BEGIN_STREET_NAMES>.\",\"10\":\"Marchez vers <CARDINAL_DIRECTION>.\",\"11\":\"Marchez vers <CARDINAL_DIRECTION> pendant <LENGTH>.\",\"12\":\"Marchez vers <CARDINAL_DIRECTION> sur <STREET_NAMES>.\",\"13\":\"Marchez vers <CARDINAL_DIRECTION> sur <STREET_NAMES> pendant <LENGTH>.\",\"14\":\"Marchez vers <CARDINAL_DIRECTION> sur <BEGIN_STREET_NAMES>.\",\"15\":\"P\\u00E9dalez vers <CARDINAL_DIRECTION>.\",\"16\":\"P\\u00E9dalez vers <CARDINAL_DIRECTION> pendant <LENGTH>.\",\"17\":\"P\\u00E9dalez vers <CARDINAL_DIRECTION> sur <STREET_NAMES>.\",\"18\":\"P\\u00E9dalez vers <CARDINAL_DIRECTION> sur <STREET_NAMES> pendant <LENGTH>.\",\"19\":\"P\\u00E9dalez vers <CARDINAL_DIRECTION> sur <BEGIN_STREET_NAMES>.\"},\"cardinal_directions\":[\"le nord\",\"le nord-est\",\"l'est\",\"le sud-est\",\"le sud\",\"le sud-est\",\"l'ouest\",\"le nord-ouest\"],\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"metric_lengths\":[\"<KILOMETERS> kilom\\u00E8tres\",\"1 kilometre\",\"<METERS> m\\u00E8tres\",\"moins de 10 m\\u00E8tres\"],\"us_customary_lengths\":[\"<MILES> miles\",\"1 mile\",\"un demi mile\",\"un quart de mile\",\"<FEET> pieds\",\"moins de 10 pieds\"],\"example_phrases\":{\"0\":[\"Head east.\",\"Head north.\"],\"1\":[\"Head east for a half mile.\",\"Head north for 1 kilometer.\"],\"2\":[\"Head southwest on 5th Avenue.\"],\"3\":[\"Head southwest on 5th Avenue for 1 tenth of a mile.\"],\"4\":[\"Head south on North Prince Street, U.S. 2 22.\"],\"5\":[\"Drive east.\",\"Drive north.\"],\"6\":[\"Drive east for a half mile.\",\"Drive north for 1 kilometer.\"],\"7\":[\"Drive southwest on 5th Avenue.\"],\"8\":[\"Drive southwest on 5th Avenue for 1 tenth of a mile.\"],\"9\":[\"Drive south on North Prince Street, U.S. 2 22.\"],\"10\":[\"Walk east.\",\"Walk north.\"],\"11\":[\"Walk east for a half mile.\",\"Walk north for 1 kilometer.\"],\"12\":[\"Walk southwest on 5th Avenue.\"],\"13\":[\"Walk southwest on 5th Avenue for 1 tenth of a mile.\"],\"14\":[\"Walk south on North Prince Street, U.S. 2 22.\"],\"15\":[\"Bike east.\",\"Bike north.\"],\"16\":[\"Bike east for a half mile.\",\"Bike north for 1 kilometer.\"],\"17\":[\"Bike southwest on 5th Avenue.\"],\"18\":[\"Bike southwest on 5th Avenue for 1 tenth of a mile.\"],\"19\":[\"Bike south on North Prince Street, U.S. 2 22.\"]}},\"transit\":{\"phrases\":{\"0\":\"Prenez <TRANSIT_NAME>. (<TRANSIT_STOP_COUNT> <TRANSIT_STOP_COUNT_LABEL>)\",\"1\":\"Prenez <TRANSIT_NAME> vers <TRANSIT_HEADSIGN>. (<TRANSIT_STOP_COUNT> <TRANSIT_STOP_COUNT_LABEL>)\"},\"empty_transit_name_labels\":[\"le tram\",\"le metro\",\"le train\",\"le bus\",\"le ferry\",\"le t\\u00E9l\\u00E9ph\\u00E9rique\",\"la gondole\",\"le funiculaire\"],\"transit_stop_count_labels\":{\"one\":\"arr\\u00EAt\",\"other\":\"arr\\u00EAts\"},\"example_phrases\":{\"0\":[\"Prenez New Haven. (1 arr\\u00EAt)\",\"Prenez le metro. (2 arr\\u00EAts)\",\"Prenez le bus. (12 arr\\u00EAts)\"],\"1\":[\"Prenez F vers JAMAICA - 179 ST. (10 arr\\u00EAts)\",\"Prenez le ferry vers Staten Island. (1 arr\\u00EAt)\"]}},\"transit_connection_destination\":{\"phrases\":{\"0\":\"Sortez de la station.\",\"1\":\"Sortez de <TRANSIT_STOP>.\",\"2\":\"Sortez de <TRANSIT_STOP> <STATION_LABEL>.\"},\"station_label\":\"Station\",\"example_phrases\":{\"0\":[\"Sortez de la station.\"],\"1\":[\"Sortez de Embarcadero Station.\"],\"2\":[\"Sortez de 8 St - NYU Station.\"]}},\"transit_connection_destination_verbal\":{\"phrases\":{\"0\":\"Sortez de la station.\",\"1\":\"Sortez de <TRANSIT_STOP>.\",\"2\":\"Sortez de <TRANSIT_STOP> <STATION_LABEL>.\"},\"station_label\":\"Station\",\"example_phrases\":{\"0\":[\"Sortez de la station.\"],\"1\":[\"Sortez de Embarcadero Station.\"],\"2\":[\"Sortez de 8 St - NYU Station.\"]}},\"transit_connection_start\":{\"phrases\":{\"0\":\"Entez dans la station.\",\"1\":\"Entez dans <TRANSIT_STOP>.\",\"2\":\"Entez dans <TRANSIT_STOP> <STATION_LABEL>.\"},\"station_label\":\"Station\",\"example_phrases\":{\"0\":[\"Entez dans la station.\"],\"1\":[\"Entez dans Embarcadero Station.\"],\"2\":[\"Entez dans 8 St - NYU Station.\"]}},\"transit_connection_start_verbal\":{\"phrases\":{\"0\":\"Entez dans la station.\",\"1\":\"Entez dans <TRANSIT_STOP>.\",\"2\":\"Entez dans <TRANSIT_STOP> <STATION_LABEL>.\"},\"station_label\":\"Station\",\"example_phrases\":{\"0\":[\"Entez dans la station.\"],\"1\":[\"Entez dans Embarcadero Station.\"],\"2\":[\"Entez dans 8 St - NYU Station.\"]}},\"transit_connection_transfer\":{\"phrases\":{\"0\":\"Changez \\u00E0 station.\",\"1\":\"Changez \\u00E0 <TRANSIT_STOP>.\",\"2\":\"Changez \\u00E0 <TRANSIT_STOP> <STATION_LABEL>.\"},\"station_label\":\"Station\",\"example_phrases\":{\"0\":[\"Changez \\u00E0 station.\"],\"1\":[\"Changez \\u00E0 Embarcadero Station.\"],\"2\":[\"Changez \\u00E0 8 St - NYU Station.\"]}},\"transit_connection_transfer_verbal\":{\"phrases\":{\"0\":\"Changez \\u00E0 station.\",\"1\":\"Changez \\u00E0 <TRANSIT_STOP>.\",\"2\":\"Changez \\u00E0 <TRANSIT_STOP> <STATION_LABEL>.\"},\"station_label\":\"Station\",\"example_phrases\":{\"0\":[\"Changez \\u00E0 station.\"],\"1\":[\"Changez \\u00E0 Embarcadero Station.\"],\"2\":[\"Changez \\u00E0 8 St - NYU Station.\"]}},\"transit_remain_on\":{\"phrases\":{\"0\":\"Restez sur <TRANSIT_NAME>. (<TRANSIT_STOP_COUNT> <TRANSIT_STOP_COUNT_LABEL>)\",\"1\":\"Restez sur <TRANSIT_NAME> vers <TRANSIT_HEADSIGN>. (<TRANSIT_STOP_COUNT> <TRANSIT_STOP_COUNT_LABEL>)\"},\"empty_transit_name_labels\":[\"le tram\",\"le metro\",\"le train\",\"le bus\",\"le ferry\",\"le t\\u00E9l\\u00E9ph\\u00E9rique\",\"la gondole\",\"le funiculaire\"],\"transit_stop_count_labels\":{\"one\":\"arr\\u00EAt\",\"other\":\"arr\\u00EAts\"},\"example_phrases\":{\"0\":[\"Restez sur New Haven. (1 arr\\u00EAt)\",\"Restez sur le train. (3 arr\\u00EAts)\"],\"1\":[\"Restez sur F vers JAMAICA - 179 ST. (10 arr\\u00EAts)\"]}},\"transit_remain_on_verbal\":{\"phrases\":{\"0\":\"Restez sur <TRANSIT_NAME>.\",\"1\":\"Restez sur <TRANSIT_NAME> vers <TRANSIT_HEADSIGN>.\"},\"empty_transit_name_labels\":[\"le tram\",\"le metro\",\"le train\",\"le bus\",\"le ferry\",\"le t\\u00E9l\\u00E9ph\\u00E9rique\",\"la gondole\",\"le funiculaire\"],\"example_phrases\":{\"0\":[\"Restez sur New Haven.\"],\"1\":[\"Restez sur F vers JAMAICA - 179 ST.\"]}},\"transit_transfer\":{\"phrases\":{\"0\":\"Changez pour prendre <TRANSIT_NAME>. (<TRANSIT_STOP_COUNT> <TRANSIT_STOP_COUNT_LABEL>)\",\"1\":\"Changez pour prendre <TRANSIT_NAME> vers <TRANSIT_HEADSIGN>. (<TRANSIT_STOP_COUNT> <TRANSIT_STOP_COUNT_LABEL>)\"},\"empty_transit_name_labels\":[\"le tram\",\"le metro\",\"le train\",\"le bus\",\"le ferry\",\"le t\\u00E9l\\u00E9ph\\u00E9rique\",\"la gondole\",\"le funiculaire\"],\"transit_stop_count_labels\":{\"one\":\"arr\\u00EAt\",\"other\":\"arr\\u00EAts\"},\"example_phrases\":{\"0\":[\"Changez pour prendre New Haven. (1 arr\\u00EAt)\",\"Changez pour prendre le tram. (4 arr\\u00EAts)\"],\"1\":[\"Changez pour prendre F vers JAMAICA - 179 ST. (10 arr\\u00EAts)\"]}},\"transit_transfer_verbal\":{\"phrases\":{\"0\":\"Changez pour prendre <TRANSIT_NAME>.\",\"1\":\"Changez pour prendre <TRANSIT_NAME> vers <TRANSIT_HEADSIGN>.\"},\"empty_transit_name_labels\":[\"le tram\",\"le metro\",\"le train\",\"le bus\",\"le ferry\",\"le t\\u00E9l\\u00E9ph\\u00E9rique\",\"la gondole\",\"le funiculaire\"],\"example_phrases\":{\"0\":[\"Changez pour prendre New Haven.\"],\"1\":[\"Changez pour prendre F vers JAMAICA - 179 ST.\"]}},\"transit_verbal\":{\"phrases\":{\"0\":\"Prenez <TRANSIT_NAME>.\",\"1\":\"Prenez <TRANSIT_NAME> vers <TRANSIT_HEADSIGN>.\"},\"empty_transit_name_labels\":[\"le tram\",\"le metro\",\"le train\",\"le bus\",\"le ferry\",\"le t\\u00E9l\\u00E9ph\\u00E9rique\",\"la gondole\",\"le funiculaire\"],\"example_phrases\":{\"0\":[\"Prenez New Haven.\"],\"1\":[\"Prenez F vers JAMAICA - 179 ST.\"]}},\"turn\":{\"phrases\":{\"0\":\"Tournez \\u00E0 <RELATIVE_DIRECTION>.\",\"1\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> dans <STREET_NAMES>.\",\"2\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> dans <BEGIN_STREET_NAMES>. Continuez sur <STREET_NAMES>.\",\"3\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES>.\",\"4\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <JUNCTION_NAME>.\",\"5\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Tournez \\u00E0 gauche.\"],\"1\":[\"Tournez \\u00E0 droite dans Flatbush Avenue.\"],\"2\":[\"Tournez \\u00E0 gauche dans North Bond Street\\/US 1 Business\\/MD 924. Continuez sur MD 924.\"],\"3\":[\"Tournez \\u00E0 droite pour rester sur Sunstone Drive.\"],\"4\":[\"Turn right at Mannenbashi East.\"],\"5\":[\"Turn left toward Baltimore.\"]}},\"turn_verbal\":{\"phrases\":{\"0\":\"Tournez \\u00E0 <RELATIVE_DIRECTION>.\",\"1\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> dans <STREET_NAMES>.\",\"2\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> dans <BEGIN_STREET_NAMES>.\",\"3\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES>.\",\"4\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <JUNCTION_NAME>.\",\"5\":\"Tournez \\u00E0 <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Turn left.\"],\"1\":[\"Tournez \\u00E0 droite dans Flatbush Avenue.\"],\"2\":[\"Tournez \\u00E0 gauche dans North Bond Street, U.S. 1 Business.\"],\"3\":[\"Tournez \\u00E0 droite pour rester sur Sunstone Drive.\"],\"4\":[\"Turn right at Mannenbashi East.\"],\"5\":[\"Turn left toward Baltimore.\"]}},\"uturn\":{\"phrases\":{\"0\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION>.\",\"1\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> dans <STREET_NAMES>.\",\"2\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES>.\",\"3\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <CROSS_STREET_NAMES>.\",\"4\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <CROSS_STREET_NAMES> dans <STREET_NAMES>.\",\"5\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <CROSS_STREET_NAMES> pour rester sur <STREET_NAMES>.\",\"6\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <JUNCTION_NAME>.\",\"7\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Faites demi-tour \\u00E0 gauche un demi-tour.\"],\"1\":[\"Faites demi-tour \\u00E0 droite dans Bunker Hill Road.\"],\"2\":[\"Faites demi-tour \\u00E0 gauche pour rester sur Bunker Hill Road.\"],\"3\":[\"Faites demi-tour \\u00E0 gauche \\u00E0 Devonshire Road.\"],\"4\":[\"Faites demi-tour \\u00E0 gauche \\u00E0 Devonshire Road dans Jonestown Road\\/US 22.\"],\"5\":[\"Faites demi-tour \\u00E0 gauche \\u00E0 Devonshire Road pour rester sur Jonestown Road\\/US 22.\"],\"6\":[\"Make a right U-turn at Mannenbashi East.\"],\"7\":[\"Make a left U-turn toward Baltimore.\"]}},\"uturn_verbal\":{\"phrases\":{\"0\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> un demi-tour.\",\"1\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> dans <STREET_NAMES>.\",\"2\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> pour rester sur <STREET_NAMES>.\",\"3\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <CROSS_STREET_NAMES>.\",\"4\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <CROSS_STREET_NAMES> dans <STREET_NAMES>.\",\"5\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <CROSS_STREET_NAMES> pour rester sur <STREET_NAMES>.\",\"6\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> \\u00E0 <JUNCTION_NAME>.\",\"7\":\"Faites demi-tour \\u00E0 <RELATIVE_DIRECTION> vers <TOWARD_SIGN>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\",\"the stairs\",\"the bridge\",\"the tunnel\"],\"relative_directions\":[\"gauche\",\"droite\"],\"example_phrases\":{\"0\":[\"Faites demi-tour \\u00E0 gauche un demi-tour.\"],\"1\":[\"Faites demi-tour \\u00E0 droite dans Bunker Hill Road.\"],\"2\":[\"Faites demi-tour \\u00E0 gauche pour rester sur Bunker Hill Road.\"],\"3\":[\"Faites demi-tour \\u00E0 gauche \\u00E0 Devonshire Road.\"],\"4\":[\"Faites demi-tour \\u00E0 gauche \\u00E0 Devonshire Road dans Jonestown Road, U.S. 22.\"],\"5\":[\"Faites demi-tour \\u00E0 gauche \\u00E0 Devonshire Road pour rester sur Jonestown Road, U.S. 22.\"],\"6\":[\"Make a right U-turn at Mannenbashi East.\"],\"7\":[\"Make a left U-turn toward Baltimore.\"]}},\"verbal_multi_cue\":{\"phrases\":{\"0\":\"<CURRENT_VERBAL_CUE> Ensuite, <NEXT_VERBAL_CUE>\",\"1\":\"<CURRENT_VERBAL_CUE> Ensuite, dans <LENGTH>, <NEXT_VERBAL_CUE>\"},\"metric_lengths\":[\"<KILOMETERS> kilom\\u00E8tres\",\"1 kilometre\",\"<METERS> m\\u00E8tres\",\"moins de 10 m\\u00E8tres\"],\"us_customary_lengths\":[\"<MILES> miles\",\"1 mile\",\"un demi mile\",\"un quart de mile\",\"<FEET> pieds\",\"moins de 10 pieds\"],\"example_phrases\":{\"0\":[\"Serrez \\u00E0 droite sur East Fayette Street. Ensuite, Tournez \\u00E0 droite vers North Gay Street.\"],\"1\":[\"Bear right onto East Fayette Street. Then, in 500 feet, Turn right onto North Gay Street.\"]}},\"approach_verbal_alert\":{\"phrases\":{\"0\":\"Dans <LENGTH>, <CURRENT_VERBAL_CUE>.\"},\"metric_lengths\":[\"<KILOMETERS> kilom\\u00E8tres\",\"1 kilometre\",\"<METERS> m\\u00E8tres\",\"moins de 10 m\\u00E8tres\"],\"us_customary_lengths\":[\"<MILES> miles\",\"1 mile\",\"un demi mile\",\"un quart de mile\",\"<FEET> pieds\",\"moins de 10 pieds\"],\"example_phrases\":{\"0\":[\"In a quarter mile, Turn right onto North Gay Street.\"]}},\"pass\":{\"phrases\":{\"0\":\"Pass <OBJECT_LABEL>.\",\"1\":\"Pass traffic signals on <OBJECT_LABEL>.\"},\"object_labels\":[\"the gate\",\"the bollards\",\"ways intersection\"],\"example_phrases\":{\"0\":[\"Pass the gate.\"]}},\"elevator\":{\"phrases\":{\"0\":\"Prenez l'ascenseur.\",\"1\":\"Prenez l'ascenseur jusqu'au niveau <LEVEL>.\"},\"example_phrases\":{\"0\":[\"Take the elevator.\"],\"1\":[\"Take the elevator to Level 1.\"]}},\"steps\":{\"phrases\":{\"0\":\"Prenez les escaliers.\",\"1\":\"Prenez les escaliers jusqu'au niveau <LEVEL>.\"},\"example_phrases\":{\"0\":[\"Take the stairs.\"],\"1\":[\"Take the stairs to Level 2.\"]}},\"escalator\":{\"phrases\":{\"0\":\"Prenez l'escalator.\",\"1\":\"Prenez l'escalator jusqu'au niveau <LEVEL>.\"},\"example_phrases\":{\"0\":[\"Take the escalator.\"],\"1\":[\"Take the escalator to Level 3.\"]}},\"enter_building\":{\"phrases\":{\"0\":\"Entrez dans le b\\u00E2timent.\",\"1\":\"Entrez dans le b\\u00E2timent et continuez sur <STREET_NAMES>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\"],\"example_phrases\":{\"0\":[\"Enter the building.\"],\"1\":[\"Enter the building, and continue on the walkway.\"]}},\"exit_building\":{\"phrases\":{\"0\":\"Sortez du b\\u00E2timent.\",\"1\":\"Sortez du b\\u00E2timent et continuez sur <STREET_NAMES>.\"},\"empty_street_name_labels\":[\"l'all\\u00E9e\",\"la piste cyclable\",\"la piste de v\\u00E9lo de montagne\",\"le passage prot\\u00E9g\\u00E9\"],\"example_phrases\":{\"0\":[\"Exit the building.\"],\"1\":[\"Exit the building, and continue on the walkway.\"]}}}}");
       LocalVectorDataSource localSource = new LocalVectorDataSource(projection);
       VectorLayer vectorLayer = new VectorLayer(localSource);
       mapView.getLayers().add(vectorLayer);
       MapPosVector vector = new MapPosVector();
       vector.add(new MapPos(5.7168, 45.1845));
       vector.add(new MapPos(5.74027, 45.24433));
       RoutingRequest request = new RoutingRequest(projection, vector);
       request.setCustomParameter("costing_options", Variant.fromString("{\"bicycle\":{\"non_network_penalty\":0,\"use_ferry\":0,\"shortest\":true,\"use_roads\":0.0,\"use_tracks\":0.5,\"bicycle_type\":\"Hybrid\"}}"));
//       request.setCustomParameter("directions_options", Variant.fromString("{\"language\":\"fr-FR\"}"));
       request.setCustomParameter("language", new Variant("fr-FR"));
       runValhallaInThread(routingService, request, "bicycle", localSource);
//            request = new RoutingRequest(projection, vector);
   }
    public static void largeLog(String tag, String content) {
        if (content.length() > 4000) {
            Log.d(tag, content.substring(0, 4000));
            largeLog(tag, content.substring(4000));
        } else {
            Log.d(tag, content);
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.second_fragment, container, false);
        com.carto.utils.Log.setShowInfo(true);
        com.carto.utils.Log.setShowDebug(true);
        com.carto.utils.Log.setShowWarn(true);
        com.carto.utils.Log.setShowError(true);

        final MapView mapView = this.mapView = (MapView) view.findViewById(R.id.mapView);
        final EPSG4326 projection = new EPSG4326();
        final Options options = mapView.getOptions();
        options.setZoomGestures(true);
        options.setRestrictedPanning(true);
        options.setSeamlessPanning(true);
        options.setRotatable(true);
//        options.setTileThreadPoolSize(1);

//        options.setRenderProjectionMode(RenderProjectionMode.RENDER_PROJECTION_MODE_SPHERICAL);
        options.setPanningMode(PanningMode.PANNING_MODE_STICKY);
        options.setBaseProjection(projection);
        mapView.setFocusPos(new MapPos(5.7279, 45.1949), 0);
        mapView.setZoom(17f, 0);
        checkStoragePermission(view);

        return view;
    }

}
