#!/bin/sh
# Off-device checks: builds the real firmware sources against the host stubs in
# stub/ and runs them. Needs a host C++ compiler, nothing from PlatformIO.
#
#   ./run.sh              run the API checks
#   ./run.sh --serve 8181 serve the real request handler on localhost instead,
#                         so the simulator (Device URL http://127.0.0.1:8181)
#                         can be driven against it in a browser
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
FW="$DIR/.."
OUT="${TMPDIR:-/tmp}/mini-lolly-hosttest"
CXX="${CXX:-c++}"
FLAGS="-std=c++17 -I $DIR/stub -I $FW/include -I $FW/src"
SRC="$FW/src/PresetStore.cpp $FW/src/effects.cpp $FW/src/fixmath.cpp"

if [ "$1" = "--serve" ]; then
  PORT="${2:-8181}"
  # A fresh store every run, or the last run's presets are what you test.
  rm -rf "$OUT/serve-nvs"
  mkdir -p "$OUT/serve-nvs"
  $CXX $FLAGS "$DIR/host_server.cpp" $SRC -o "$OUT/host_server"
  exec "$OUT/host_server" "$OUT/serve-nvs" "$PORT"
fi

rm -rf "$OUT/nvs"
mkdir -p "$OUT/nvs"
$CXX $FLAGS "$DIR/test_api.cpp" $SRC -o "$OUT/test_api"
"$OUT/test_api" "$OUT/nvs"
