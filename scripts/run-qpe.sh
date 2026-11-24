#!/bin/sh
# Launch Qt Extended (Qtopia) natively on LoongArch with the VNC backend.
#
# Place this script in <build-root>/scripts/ (next to the `install/` tree that
# `qbuild image` produced).  It resolves the build root relative to itself so
# it works regardless of where the repo was checked out.
#
# Usage:  setsid ./scripts/run-qpe.sh &
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE=$(dirname -- "$SCRIPT_DIR")
P=$BASE/install
export QPEDIR=$P
export HOME=$BASE/qpe-home
export LD_LIBRARY_PATH=$P/lib:$P/qt_plugins/gfxdrivers
export QT_PLUGIN_PATH=$P/qt_plugins
export QWS_DISPLAY='VNC:0'
export QWS_SIZE=640x480   # must match viewer's expected desktop size
mkdir -p "$HOME"
exec "$P/bin/qpe" -qws "$@"
