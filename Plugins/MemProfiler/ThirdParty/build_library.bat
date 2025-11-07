set TOOLCHAIN="E:\AndroidSDK\ndk\25.1.8937393\build\cmake\android.toolchain.cmake"
set PROGARM="E:\AndroidSDK\cmake\4.1.1\bin\ninja.exe"
set ABI=arm64-v8a
set PLATFORM=android-21

cd libunwindstack

rmdir /s /q build
mkdir build
cd build
cmake .. -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=%TOOLCHAIN% -DANDROID_ABI=%ABI% -DANDROID_PLATFORM=%PLATFORM% -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=%PROGARM%
cmake --build . --config Release

xcopy .\libunwindstack.so ..\..\..\Source\MemProfiler\libs\ /Y
