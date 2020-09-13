#!/bin/sh

OSTYPE=$(uname)

if [ "${OSTYPE}" != "Darwin" ]; then
    echo "[obs-overlay-plugin - Error] macOS install dependencies script can be run on Darwin-type OS only."
    exit 1
fi

HAS_BREW=$(type brew 2>/dev/null)

if [ "${HAS_BREW}" = "" ]; then
    echo "[obs-overlay-plugin - Error] Please install Homebrew (https://www.brew.sh/) to build obs-overlay-plugin on macOS."
    exit 1
fi

# OBS Studio deps
echo "[obs-overlay-plugin] Updating Homebrew.."
brew update >/dev/null
echo "[obs-overlay-plugin] Checking installed Homebrew formulas.."
BREW_PACKAGES=$(brew list)
BREW_DEPENDENCIES="ffmpeg libav cmake"

for DEPENDENCY in ${BREW_DEPENDENCIES}; do
    if echo "${BREW_PACKAGES}" | grep -q "^${DEPENDENCY}\$"; then
        echo "[obs-overlay-plugin] Upgrading OBS-Studio dependency '${DEPENDENCY}'.."
        brew upgrade ${DEPENDENCY} 2>/dev/null
    else
        echo "[obs-overlay-plugin] Installing OBS-Studio dependency '${DEPENDENCY}'.."
        brew install ${DEPENDENCY} 2>/dev/null
    fi
done

# =!= NOTICE =!=
# When building QT5 from sources on macOS 10.13+, use local qt5 formula:
# brew install ./CI/macos/qt.rb
# Pouring from the bottle is much quicker though, so use bottle for now.
# =!= NOTICE =!=

brew install https://gist.githubusercontent.com/DDRBoxman/b3956fab6073335a4bf151db0dcbd4ad/raw/ed1342a8a86793ea8c10d8b4d712a654da121ace/qt.rb

# Pin this version of QT5 to avoid `brew upgrade`
# upgrading it to incompatible version
brew pin qt

# Fetch and install Packages app
# =!= NOTICE =!=
# Installs a LaunchDaemon under /Library/LaunchDaemons/fr.whitebox.packages.build.dispatcher.plist
# =!= NOTICE =!=

HAS_PACKAGES=$(type packagesbuild 2>/dev/null)

if [ "${HAS_PACKAGES}" = "" ]; then
    echo "[obs-overlay-plugin] Installing Packaging app (might require password due to 'sudo').."
    curl -o './Packages.pkg' --retry-connrefused -s --retry-delay 1 'https://s3-us-west-2.amazonaws.com/obs-nightly/Packages.pkg'
    sudo installer -pkg ./Packages.pkg -target /
fi 
