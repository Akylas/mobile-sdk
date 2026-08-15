#!/bin/bash 

# Doxygen directory
doxygenDir="/usr/local/bin"

# Set dirs
cmdDir=$(dirname $0)
baseDir="${cmdDir}/.."
tempDir="${baseDir}/build/doxygen"
distDir="${baseDir}/dist/ios"

# Copy proxy files to temp directory
rm -rf ${tempDir}
mkdir -p ${tempDir}
cp -r ${baseDir}/generated/ios-objc/proxies/* ${tempDir}
cp -r ${baseDir}/ios/objc/ui/MapView.h ${tempDir}/MSFMapView.h
cp -r ${baseDir}/ios/objc/ui/MapView.mm ${tempDir}/MSFMapView.mm
find ${tempDir} -name "*MSFBaseMapView.*" -exec rm {} \;
find ${tempDir} -name "*MSFRedrawRequestListener.*" -exec rm {} \;
find ${tempDir} -name "*MSFIOSUtils.*" -exec rm {} \;

# Remove attributes unsupported by doxygen from source files
find ${tempDir} -name "*.h" -exec sed -i '' 's/__attribute__ ((visibility("default")))//g' {} \;

# Execute doxygen
rm -rf ${distDir}/docObjC
${doxygenDir}/doxygen "doxygen/doxygen-objc.conf"

# Finished
echo "Done!"
