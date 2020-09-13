#!/bin/sh

set -e

OSTYPE=$(uname)

if [ "${OSTYPE}" != "Darwin" ]; then
    echo "[obs-overlay-plugin - Error] macOS package script can be run on Darwin-type OS only."
    exit 1
fi

echo "[obs-overlay-plugin] Preparing package build"

GIT_HASH=$(git rev-parse --short HEAD)
GIT_BRANCH_OR_TAG=$(git name-rev --name-only HEAD | awk -F/ '{print $NF}')

VERSION="$GIT_HASH-$GIT_BRANCH_OR_TAG"
LATEST_VERSION="$GIT_BRANCH_OR_TAG"

FILENAME_UNSIGNED="obs-overlay-source-$VERSION-Unsigned.pkg"
FILENAME="obs-overlay-source-$VERSION.pkg"

echo "-- Modifying obs-overlay-source.so"
install_name_tool \
	-change /tmp/obsdeps/bin/libavcodec.58.dylib @rpath/libavcodec.58.dylib \
	-change /tmp/obsdeps/bin/libavutil.56.dylib @rpath/libavutil.56.dylib \
	./build/obs-overlay-source.so

echo "-- Dependencies for obs-overlay-source"
otool -L ./build/obs-overlay-source.so

if [[ "$RELEASE_MODE" == "True" ]]; then
	echo "-- Signing plugin binary: obs-overlay-source.so"
	codesign --sign "$CODE_SIGNING_IDENTITY" ./build/obs-overlay-source.so
else
	echo "-- Skipped plugin codesigning"
fi

echo "-- Actual package build"
packagesbuild ./CI/macos/obs-overlay-source.pkgproj

echo "-- Renaming obs-overlay-source.pkg to $FILENAME_UNSIGNED"
# mkdir release
mv ./release/obs-overlay-source.pkg ./release/$FILENAME_UNSIGNED

if [[ "$RELEASE_MODE" == "True" ]]; then
	echo "[obs-overlay-source] Signing installer: $FILENAME"
	productsign \
		--sign "$INSTALLER_SIGNING_IDENTITY" \
		./release/$FILENAME_UNSIGNED \
		./release/$FILENAME

	echo "[obs-overlay-source] Submitting installer $FILENAME for notarization"
	zip -r ./release/$FILENAME.zip ./release/$FILENAME
	UPLOAD_RESULT=$(xcrun altool \
		--notarize-app \
		--primary-bundle-id "com.grantjbutler.obs-overlay-source.pkg" \
		--username "$AC_USERNAME" \
		--password "$AC_PASSWORD" \
		--asc-provider "$AC_PROVIDER_SHORTNAME" \
		--file "./release/$FILENAME.zip")
	rm ./release/$FILENAME.zip

	REQUEST_UUID=$(echo $UPLOAD_RESULT | awk -F ' = ' '/RequestUUID/ {print $2}')
	echo "Request UUID: $REQUEST_UUID"

	echo "[obs-overlay-source] Wait for notarization result"
	# Pieces of code borrowed from rednoah/notarized-app
	while sleep 30 && date; do
		CHECK_RESULT=$(xcrun altool \
			--notarization-info "$REQUEST_UUID" \
			--username "$AC_USERNAME" \
			--password "$AC_PASSWORD" \
			--asc-provider "$AC_PROVIDER_SHORTNAME")
		echo $CHECK_RESULT

		if ! grep -q "Status: in progress" <<< "$CHECK_RESULT"; then
			echo "[obs-overlay-source] Staple ticket to installer: $FILENAME"
			xcrun stapler staple ./release/$FILENAME
			break
		fi
	done
else
	echo "[obs-overlay-source] Skipped installer codesigning and notarization"
fi