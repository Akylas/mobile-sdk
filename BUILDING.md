# Building CARTO Mobile SDK 

**We strongly suggest to use the precompiled SDK versions that can be found in
the [Releases](https://github.com/CartoDB/mobile-sdk/releases) section** 

Getting all the SDK dependencies resolved and waiting for the build to complete can be very time-consuming.

## Dependencies
The following instructions assume **Linux** or **MacOS** operating system. For Windows-based builds we
recommend first installing **Windows Subsystem for Linux (WSL)**. Once installed, the following
instructions can be used on Windows also. Otherwise minor changes are needed, like using 
`mklink /D` instead of `ln -s` in the following instructions.

We assume command-line versions of **git**, **unzip** and **curl** are already installed.

Use `git submodule` to resolve source-level dependencies:

```
git submodule update --init --remote --recursive
```

Download and set up 'boost' library:

```
wget -q --show-progress -O boost_1_89_0.zip https://sourceforge.net/projects/boost/files/boost/1.89.0/boost_1_89_0.zip
unzip -q boost_1_89_0.zip
cd libs-external
ln -s ../boost_1_89_0 boost
cd ../boost_1_89_0
./bootstrap.sh
./b2 headers
cd ..
```

Special **swig** version (swig-2.0.11-nutiteq branch) is needed for generating language-specific wrappers, this can be downloaded from https://github.com/CartoDB/swig. Clone it and compile it using usual `./autogen.sh; ./configure; make` routine. Make sure build script refers to this one.

```
brew install autoconf automake libtool
git clone https://github.com/farfromrefug/mobile-swig.git
cd mobile-swig
wget https://github.com/PhilipHazel/pcre2/releases/download/pcre2-10.44/pcre2-10.44.tar.gz
./Tools/pcre-build.sh
./autogen.sh
./configure
make
```

**Python 2.7.x** or **Python 3.x** is used for build scripts

**CMake 3.14 or later** is required by build scripts

Android build requires **Android SDK**, **JDK 10** or newer and **Android NDK r22** or newer.

iOS build requires **XCode 12** or later.

Universal Windows Platform build requires **Visual Studio 2022** and **Microsoft Windows SDK**.

## SDK profiles
CARTO Mobile SDK can be compiled with different features. The feature set is defined by **profiles**,
which are defined in 'scripts/build/sdk_profiles.json' file. Different profiles can be combined, for
example the official SDK builds are currently compiled with 'valhalla+nmlmodellodtree' profiles. The
following instructions use 'standard' profile as an example.

In order to make SDK binaries as small as possible, 'lite' profile can be used. This profile disables
geocoding, routing and offline support, but resulting binaries are about 40% smaller.

## Building process
Be patient - full build will take 1+ hours. You can speed it up by limiting architectures and platforms where it is built.

Go to 'scripts' library where the actual build scripts are located:

```
cd scripts
```

## Android build 
```
python swigpp-java.py --profile standard --swig ../mobile-swig/swig
python build-android.py --profile standard --build-aar --configuration=Release
```

## iOS build
```
python swigpp-objc.py --profile standard --swig ../mobile-swig/swig
python build-ios.py --profile standard
```

## Xamarin Android build
```
python swigpp-csharp.py --profile standard android --swig ../mobile-swig/swig
python build-xamarin.py --profile standard android
```

## Xamarin iOS build
```
python swigpp-csharp.py --profile standard ios --swig ../mobile-swig/swig
python build-xamarin.py --profile standard ios
```

## Universal Windows Platform build
```
python swigpp-csharp.py --profile standard winphone --swig ../mobile-swig/swig
python build-winphone.py --profile standard
```

# Usage
* Developer docs: https://carto.com/docs/carto-engine/mobile-sdk/
* Android sample app: https://github.com/CartoDB/mobile-android-samples
* iOS sample app: https://github.com/CartoDB/mobile-ios-samples
* .NET (Xamarin and UWP) sample app: https://github.com/CartoDB/mobile-dotnet-samples
* Scripts for preparing offline packages: https://github.com/nutiteq/mobile-sdk-scripts

# Support, Questions?
* Post an [issue](https://github.com/CartoDB/mobile-sdk/issues) to this project, submit a [Pull Request](https://github.com/CartoDB/mobile-sdk/pulls)
* Commercial support options: sales@carto.com


# Building css2xml
 Go into `libs-carto/cartocss/util`
 Then run :
 ```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cd build
make -j $(( $(nproc) + 1 )) css2xml
```