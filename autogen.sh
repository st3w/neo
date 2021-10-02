#!/bin/sh
autoreconf -f --install
automake --add-missing --copy >/dev/null 2>&1
