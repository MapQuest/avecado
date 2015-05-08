#!/bin/bash

set -e

git submodule status | cut -c 1-1 \
    | xargs -i% test % != "-" || \
    ( echo "missing submodules, try: git submodule update --init --recursive" ; exit 1)

libtoolize
aclocal -I m4
autoreconf --force --install

echo "Ready to run ./configure"
