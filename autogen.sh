#!/bin/bash

set -e

libtoolize
aclocal
autoconf
autoheader
automake --add-missing
