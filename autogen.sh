#!/bin/bash
libtoolize
aclocal
autoconf
touch AUTHORS NEWS README ChangeLog
automake --add-missing
