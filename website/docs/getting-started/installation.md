---
title: Installation
sidebar_position: 1
---

# Installation

The Akylas fork is published under **new artifact coordinates** — the API namespace stays
`com.massifmaps.*`, but the packages come from the fork, not from Massif's original distribution.

:::tip Version
Always use the latest version from the
[Releases page](https://github.com/Akylas/mobile-sdk/releases). The `5.x` line below is an example.
:::

## Android

Add JitPack and the dependency to your app's `build.gradle`:

```groovy
repositories {
    mavenCentral()
    maven { url 'https://jitpack.io' }
}

dependencies {
    implementation 'com.github.Akylas:mobile-sdk-android-aar:5.0.0'
}
```

Add the INTERNET permission to `AndroidManifest.xml` (needed for online tiles/services):

```xml
<uses-permission android:name="android.permission.INTERNET"/>
```

## iOS

Use Swift Package Manager:

1. In Xcode: **File → Add Packages…**
2. Paste the package URL:

   ```
   https://github.com/Akylas/mobile-sdk-ios-swift
   ```
3. Pick a version and add it to your target.

You can also download a prebuilt framework from the
[Releases page](https://github.com/Akylas/mobile-sdk/releases).

## Registering a license

The original CARTO SDK required a license key registered at startup:

```java
MapView.registerLicense("YOUR_LICENSE_KEY", context);
```

```swift
MapView.registerLicense("YOUR_LICENSE_KEY")
```

The fork does not depend on Massif's license servers for offline/self-hosted use. If you use
CARTO online services (Maps API / SQL API) you still need valid CARTO credentials — see
[CARTO Integrations](/docs/guides/carto-integrations). For fully offline maps you can proceed
without registering a license.

## Building from source

The SDK is a large C++ project built per platform. See
[`BUILDING.md`](https://github.com/Akylas/mobile-sdk/blob/master/BUILDING.md) in the repo for the
full toolchain (a SWIG fork and a boost symlink are required). A typical full build takes an hour
or more; for most apps the prebuilt artifacts above are what you want.

## Standalone routing library

If you only need routing (Valhalla) without the map view, the repo also ships a lightweight
**`routing-lib`**. See the [Routing guide](/docs/guides/routing) and the repository README.

## Next step

→ [Show your first map](/docs/getting-started/your-first-map)
