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

# This script converts image/$1 to thumb/$1

echo _THUMB $1

# ImageMagick and jpegtools
convert image/$1 -filter lanczos -geometry ${CI_TW}x${CI_TH} pnm:- | cjpeg -quality 90 -optimize -dct float -outfile thumb/$1

# jpegtools and pnmtools
# djpeg image/$1 | pamscale -w $CI_TW -h $CI_TH -filter sinc | cjpeg -quality 90 -optimize -dct float -outfile thumb/$1

