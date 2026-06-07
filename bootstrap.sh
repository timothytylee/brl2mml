#!/bin/sh
rm -rf mxml-2.9
tar -zxf mxml-2.9.tar.gz
patch -d mxml-2.9 -p 1 -i ../mxml-2.9-mingw.patch
autoreconf --verbose --force --no-recursive --install
