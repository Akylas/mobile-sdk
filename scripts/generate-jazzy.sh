#!/bin/bash 

# Set dirs
cmdDir=$(dirname $0)
baseDir="${cmdDir}/.."
tempDir="${baseDir}/build/jazzy"
distDir="${baseDir}/dist/ios"

# Copy proxy files to temp directory
rm -rf ${tempDir}
mkdir -p ${tempDir}
mkdir -p ${tempDir}/ui
mkdir -p ${tempDir}/utils
sed -e 's|MassifMaps/||g' ${distDir}/MassifMaps.framework/Headers/MassifMaps.h | tr '<>' '""' > ${tempDir}/MassifMaps.h
cp -r ${baseDir}/generated/ios-objc/proxies/* ${tempDir}
cp -r ${baseDir}/ios/objc/ui/MapView.h ${tempDir}/ui/MapView.h
cp -r ${baseDir}/ios/objc/ui/MapView.mm ${tempDir}/ui/MapView.mm
cp -r ${baseDir}/ios/objc/utils/ExceptionWrapper.h ${tempDir}/utils/ExceptionWrapper.h
cp -r ${baseDir}/ios/objc/utils/ExceptionWrapper.mm ${tempDir}/utils/ExceptionWrapper.mm
find ${tempDir} -name "*MSFIOSUtils.*" -exec rm {} \;
find ${tempDir} -name "*.h" -exec sed -i '' "s/@throws/@warning Throws/g" {} +

# Execute Jazzy
rm -rf ${distDir}/docObjC
jazzy --clean --author "Massif Maps" --author_url https://massif-maps.github.io/MassifMaps/ --github_url https://github.com/massif-maps/MassifMaps --module MassifMaps --output ${distDir}/docObjC --umbrella-header ${tempDir}/MassifMaps.h --objc --sdk iphonesimulator

# Finished
echo "Done!"
