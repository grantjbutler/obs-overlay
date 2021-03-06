#!/bin/sh

OSTYPE=$(uname)

if [ "${OSTYPE}" != "Darwin" ]; then
    echo "[obs-overlay-plugin - Error] macOS obs-studio build script can be run on Darwin-type OS only."
    exit 1
fi

HAS_CMAKE=$(type cmake 2>/dev/null)
HAS_GIT=$(type git 2>/dev/null)

if [ "${HAS_CMAKE}" = "" ]; then
    echo "[obs-overlay-plugin - Error] CMake not installed - please run 'install-dependencies-macos.sh' first."
    exit 1
fi

if [ "${HAS_GIT}" = "" ]; then
    echo "[obs-overlay-plugin - Error] Git not installed - please install Xcode developer tools or via Homebrew."
    exit 1
fi

echo "[obs-overlay-plugin] Downloading and unpacking OBS dependencies"
wget --quiet --retry-connrefused --waitretry=1 https://github.com/obsproject/obs-deps/releases/download/2020-04-24/osx-deps-2020-04-24.tar.gz
tar -xf ./osx-deps-2020-04-24.tar.gz -C /tmp

# Build obs-studio
cd ..
echo "[obs-overlay-plugin] Cloning obs-studio from GitHub.."
git clone https://github.com/obsproject/obs-studio
cd obs-studio
# OBSLatestTag=$(git describe --tags --abbrev=0)
# git checkout $OBSLatestTag
git checkout 25.0.8
mkdir build && cd build
echo "[obs-overlay-plugin] Building obs-studio.."
cmake .. \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 \
	-DDISABLE_PLUGINS=true \
    -DENABLE_SCRIPTING=0 \
	-DDepsPath=/tmp/obsdeps \
	-DCMAKE_PREFIX_PATH=/usr/local/opt/qt/lib/cmake \
&& make -j4
