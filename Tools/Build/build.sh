

cd ../..

rm -rf Release
mkdir Release

cd MemoryTraceParser
rm -rf build
mkdir build
cd build
cmake ..
cmake --build . --config Release

if [ $? -ne 0 ]; then
    echo "MemoryTraceParser build failed"
    exit 1
fi

cp MemoryTraceParser ../../Release/MemoryTraceParser
