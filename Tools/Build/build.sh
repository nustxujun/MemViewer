

cd ../..

rm -rf Release
mkdir Release

cd MemoryTraceParser
rm -rf build
mkdir build
cd build
cmake .. -G Xcode
cmake --build . --config Release

if [ $? -ne 0 ]; then
    echo "MemoryTraceParser build failed"
    exit 1
fi

cp Release/MemoryTraceParser ../../Release/MemoryTraceParser
