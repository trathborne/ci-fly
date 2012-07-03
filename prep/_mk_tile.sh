#!/bin/sh

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

# This script runs metapixel on image/$1,
# creating tiled/$1 and _lists/$1

echo _TILE $1

exec metapixel --search=global --width=$CI_MW --height=$CI_MH --library _library --out _lists/$1 --metapixel image/$1 tiled/$1
