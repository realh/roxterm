#!/bin/sh

# This script generates images etc that should be present in the release but
# not in git, which aren't catered for very well by meson.

cd `dirname "$0"`

INKSCAPE=`which inkscape`
RSVG=`which rsvg`
if [ -z "$RSVG" ]; then
    RSVG=`which rsvg-convert`
fi
if [ -z "$INKSCAPE" -a -z "$RSVG" ]; then
    echo "Need inkscape or rsvg" >&2
    exit 1
fi
if [ ! -z "$RSVG" ]; then
    CONVERT_SVG="$RSVG -w 64 -h 64 -f png -o"
else
    CONVERT_SVG="$INKSCAPE -w 64 -h 64 -e"
fi

CONVERT=`which convert`
COMPOSITE=`which composite`
if [ -z "$CONVERT" -o -z "$COMPOSITE" ]; then 
    echo "Need convert and composite from ImageMagick"
    exit 1
fi

XSLTPROC=`which xsltproc`
if [ -z "$XSLTPROC" ]; then 
    echo "Need xsltproc"
fi

make -f preconf.mk CONVERT="$CONVERT" COMPOSITE="$COMPOSITE" \
    CONVERT_SVG="$CONVERT_SVG" XSLTPROC="$XSLTPROC"
