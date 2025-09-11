#!/bin/sh
RELEASES="focal jammy noble plucky questing"
set -e
cd $(dirname $0)
rm -rf build
./AppRun --configure
VERSION=$(./version.sh)
make -C build dist
mkdir build/deb
ln build/roxterm-${VERSION}.tar.xz build/deb/roxterm_${VERSION}.orig.tar.xz
cd build/deb
tar xf roxterm_${VERSION}.orig.tar.xz
cp -r ../../debian roxterm-${VERSION}/
for REL in $RELEASES; do
    sed -e "1croxterm (${VERSION}-1ppa1${REL}1) ${REL}; urgency=low" \
        ../../debian/changelog > roxterm-${VERSION}/debian/changelog
    cd roxterm-${VERSION}
    dpkg-buildpackage -S
    cd ..
    dput ppa roxterm_${VERSION}-1ppa1${REL}1_source.changes
    rm *${REL}*
done
