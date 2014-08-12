#!/bin/bash

aclocal -I m4
autoheader
automake -a
autoconf
