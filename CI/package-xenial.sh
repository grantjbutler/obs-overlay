#!/bin/sh

set -e

cd /root/obs-overlay

export GIT_HASH=$(git rev-parse --short HEAD)
export PKG_VERSION="1-$GIT_HASH-$TRAVIS_BRANCH-git"

if [ -n "${TRAVIS_TAG}" ]; then
	export PKG_VERSION="$TRAVIS_TAG"
fi

cd /root/obs-overlay/build

PAGER=cat checkinstall -y --type=debian --fstrans=no --nodoc \
	--backup=no --deldoc=yes --install=no \
	--pkgname=obs-overlay --pkgversion="$PKG_VERSION" \
	--pkglicense="GPLv2.0" --maintainer="contact@slepin.fr" \
	--pkggroup="video" \
	--pkgsource="https://github.com/grantjbutler/obs-overlay" \
	--pakdir="/package"

chmod ao+r /package/*
