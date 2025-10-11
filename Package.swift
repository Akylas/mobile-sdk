// swift-tools-version:5.4.0

import PackageDescription

let package = Package(
    name: "CartoMobileSDK",
    platforms: [
        .iOS(.v12)
    ],
    products: [
        .library(
            name: "CartoMobileSDK",
            targets: ["CartoMobileSDK"]
        )
    ],
    dependencies: [
    ],
    targets: [
        .binaryTarget(
            name: "CartoMobileSDK",
            url: "https://github.com/Akylas/mobile-sdk/releases/download/v5.0.0-rc.13/carto-mobile-sdk-ios-metal-5.0.0-rc.13.zip",
            checksum: "sha256:7aa17b4507e7aa24b75e87a6209c5db94985ef5cd9ae3460b234e0198c3dc07c"
        )
    ]
)
