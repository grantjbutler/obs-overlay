obs-overlay
==============
Use your iPad and Apple Pencil to draw over your OBS stream.

To use this you use the (still in development) accompanying iOS app that provides a drawing canvas, and streams the canvas to OBS.

## Downloads

Binaries for Windows and Mac are available in the [Releases](https://github.com/grantjbutler/obs-overlay/releases) section.

## Building

You can run the CI scripts to build it. They will clone and build OBS Studio prior to building this plugin.

    ./CI/install-dependencies-macos.sh
    ./CI/install-build-obs-macos.sh
    ./CI/build-macos.sh
    ./CI/package-macos.sh


## Special thanks
- This project is basically forked from [obs-ios-camera-source](https://github.com/wtsnz/obs-ios-camera-source/) to provide the foundation of the plugin. This project would not exist without their prior work.
