

cd ..
python3 -m venv env
. env/bin/activate
pip3 install -r requirements.txt

cd ..

# rm -rf Release
mkdir Release

cd MemoryTraceParser
rm -rf build
mkdir build
cd build
# cmake .. -G Xcode
cmake ..
cmake --build . --config Release

if [ $? -ne 0 ]; then
    echo "MemoryTraceParser build failed"
    exit 1
fi

cp -f MemoryTraceParser ../../Release/MemoryTraceParser

cd ../..
pyinstaller --onefile "Tools/Dump/dump_ios/dump_ios.py" --distpath "Release" --workpath "Temp/build"


deactivate

rm -rf Tools/env