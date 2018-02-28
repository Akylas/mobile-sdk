set -e

rvm get head

echo '---- Setting up boost ----'
cd libs-external/boost
./bootstrap.sh
./b2 headers
cd ../..

echo '---- Downloading and installing CMake ----'
curl -o cmake-3.5.2.tar.gz -L https://cmake.org/files/v3.5/cmake-3.5.2.tar.gz
rm -rf cmake-3.5.2
tar xpfz cmake-3.5.2.tar.gz
cd cmake-3.5.2
./configure --prefix=`pwd`/dist
make
make install
export PATH=$PWD/dist/bin:$PATH
cd ..

echo '---- Downloading and installing Swig ----'
rm -rf mobile-swig
git clone https://github.com/CartoDB/mobile-swig.git
cd mobile-swig
cd pcre
aclocal
automake
./configure --prefix=`pwd`/pcre-swig-install --disable-shared
make
make install
cd ..
./autogen.sh
./configure --disable-ccache --prefix=`pwd`/dist
make
make install || true
export PATH=$PWD/dist/bin:$PATH
cd ..

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
  echo '---- Downloading and installing Android NDK r12b ----'
  curl -L http://dl.google.com/android/repository/android-ndk-r12b-linux-x86_64.zip -O
  rm -r -f android-ndk-r12b
  unzip -q android-ndk-r12b-linux-x86_64.zip
  rm android-ndk-r12b-linux-x86_64.zip
  export ANDROID_NDK_HOME=`pwd`/android-ndk-r12b;
#  export ANDROID_HOME=/usr/local/android-sdk
fi
