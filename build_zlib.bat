@echo off
git submodule update --init External/src/zlib
mkdir External\build\zlib
pushd External\build\zlib
rm -rf *
cmake -DCMAKE_INSTALL_PREFIX=../../Windows -G Ninja -DBUILD_SHARED_LIBS=off ../../src/zlib
cmake --build . --config release --target install
cd ../../
rm -rf build
popd