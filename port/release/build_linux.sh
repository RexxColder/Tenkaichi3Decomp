#!/bin/sh
# Builds the Linux release in the container of port/release/Dockerfile (Ubuntu 22.04), so that it runs on other
# people's machines:   port/release/build_linux.sh   ->  port/build_release/bt3-port-linux-x64.zip
# Needs docker, and on the host: gamedata/ (your unpacked disc) and one host build (for the data tables'
# assembly sources in port/build/gen/data and the compiled shaders, which need tools the container does not have).
set -e
cd "$(dirname "$0")/../.."
R=port/build_release
[ -d port/build/gen/data ] || { echo "run the host build first (gen_data.py, undefined.py)"; exit 1; }
docker image inspect bt3-port-release:jammy >/dev/null 2>&1 || docker build -t bt3-port-release:jammy port/release
mkdir -p $R/gen/data $R/gen/gs
cp port/build/gen/data/*.s $R/gen/data/
# the one binary block the data sources include by path (the VU1 microprograms): into the build folder, by its
# path inside the container
BLOB=$(sed -n 's/.*\.incbin "\([^"]*\)".*/\1/p' port/build/gen/data/vu1_micro.data.s | head -1)
cp "$BLOB" $R/gen/data/vu1_micro.bin
sed -i "s|\.incbin \"[^\"]*\"|.incbin \"/src/port/build/gen/data/vu1_micro.bin\"|" $R/gen/data/vu1_micro.data.s
cp -p port/build/gen/gs/*.spv $R/gen/gs/
# the repository at /src, with the release's own build folder in the place of port/build; files owned by you
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -e BT3_CC=clang64 \
    -v "$PWD":/src -v "$PWD/$R":/src/port/build -w /src bt3-port-release:jammy sh -ec '
        python3 port/tools/undefined.py | grep -E "^FAILED|objects," || true
        python3 port/tools/link.py
        sh port/setup/build.sh
        python3 port/tools/package.py'
echo "release: $R/bt3-port-linux-x64.zip"
