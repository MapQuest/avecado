#!/bin/bash

aclocal -I m4
autoheader
libtoolize -i
automake -a
autoconf
