#!/bin/bash

cd ../assets
xxd -i default-background.png > ../all/native/assets/DefaultBackgroundPNG.h
xxd -i default-line.png > ../all/native/assets/DefaultLinePNG.h
xxd -i default-marker.png > ../all/native/assets/DefaultMarkerPNG.h
xxd -i default-point.png > ../all/native/assets/DefaultPointPNG.h
xxd -i default-polygon.png > ../all/native/assets/DefaultPolygonPNG.h
xxd -i default-nmlmodel.nml > ../all/native/assets/DefaultNMLModelNML.h
xxd -i massifstyles-v2.zip | sed 's/unsigned/static unsigned/' > ../all/native/assets/MassifStylesV2ZIP.h
