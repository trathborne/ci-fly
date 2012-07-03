#!/bin/bash

#   This script is part of
#   Continuous Imaging 'fly'
#   Copyright (C) 2008-2012  David Lowy & Tom Rathborne
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

# This script processes image/*.jpg into thumb/*.jpg and cidb.db

# ---- SETTINGS

# The images should all have the same aspect ratio.
# MW and MH should match the aspect ratio of the images.
CI_MW=32 # Mosaic Width, # of sub-images
CI_MH=24 # Mosaic Height, # of sub-images

# ((# of images) * CI_TW * CI_TH * 3)
#     should fit in your available texture memory!
CI_TW=96 # Thumb Width, in pixels
CI_TH=72 # Thumb Height, in pixels

PROCS=2 # Number of processes to use via xargs

VRAM_MAX=$((144 * 1048576)) # Video memory in bytes

# ---- END SETTINGS

export CI_MW
export CI_MH

export CI_TW
export CI_TH

PREP_DIR=`dirname $0`

echo ---- Calculating size

ICOUNT=`ls image | grep .jpg$ | wc -l`
IVSIZE=$(( $ICOUNT * $CI_TW * $CI_TH * 3 ))

echo "Estimated $IVSIZE bytes for $ICOUNT ${CI_TW}x${CI_TH} 24-bit textures."
echo "VRAM max: $VRAM_MAX"

if [ $IVSIZE -gt $VRAM_MAX ] 
then
    echo "Size estimate greater that VRAM max. Aborting."
    echo ---- abort
    exit
fi

echo ---- Metapixel library
rm -rf _library
mkdir _library
metapixel-prepare --width=$CI_MW --height=$CI_MH image _library

echo ---- Metapixel search
rm -rf _lists tiled
mkdir _lists tiled
ls image | grep .jpg$ | xargs -n 1 -P $PROCS $PREP_DIR/_mk_tile.sh

echo ---- Lists to DB
rm -f cidb.db
ls image | grep .jpg$ | sed -e 's/.jpg$//' | $PREP_DIR/_mk_lists2scidb.pl

echo ---- Make thumbs
rm -rf thumb
mkdir thumb
ls image | grep .jpg$ | xargs -n 1 -P $PROCS $PREP_DIR/_mk_thumb.sh

echo ---- Done!

# rm -rf _library _lists tiled
# exit
echo
echo You may now remove temporary data with:
echo     rm -rf _library _lists tiled
echo Edit $0 if you would like this to happen automatically.

