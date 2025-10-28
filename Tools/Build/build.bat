@echo off
set world_space=%cd%


cd ..

call python3 -m venv env
call .\env\Scripts\activate
pip3 install -r requirements.txt


cd ..
@REM rmdir /s /q Release
mkdir Release

cd MemViewer
rmdir /s /q build
mkdir build
cd build
cmake ..
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo failed to build MemViewer
    exit /b 1
)


xcopy .\Release\MemViewer.exe ..\..\Release\ /Y


cd ../..
cd MemoryTraceParser
rmdir /s /q build
mkdir build
cd build
cmake ..
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo failed to build MemoryTraceParser
    exit /b 1
)

xcopy .\Release\MemoryTraceParser.exe ..\..\Release\ /Y


cd ../..
cd Tools/generate_symbols_windows
rmdir /s /q build
mkdir build
cd build
cmake ..
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo failed to build generate_symbols_windows
    exit /b 1
)


xcopy .\Release\gensym.exe ..\..\..\Release\ /Y

cd ../../..
echo %cd%
rmdir /s /q build

pyinstaller --onefile "Tools/Dump/dump_windows.py" --distpath "Release" --workpath "Temp/build"

if %ERRORLEVEL% NEQ 0 (
    echo failed to build dump_windows
    exit /b 1
)


call deactivate
rmdir /s /q Tools/env

pause