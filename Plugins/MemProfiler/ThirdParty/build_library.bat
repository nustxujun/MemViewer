set TOOLCHAIN="E:\AndroidSDK\ndk\25.1.8937393\build\cmake\android.toolchain.cmake"
set PROGARM="E:\AndroidSDK\cmake\4.1.1\bin\ninja.exe"
set ABI=arm64-v8a
set PLATFORM=android-21

cd libunwindstack

@REM rmdir /s /q build
@REM mkdir build
cd build
@REM cmake .. -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=%TOOLCHAIN% -DANDROID_ABI=%ABI% -DANDROID_PLATFORM=%PLATFORM% -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=%PROGARM%
cmake --build . --config Release

xcopy .\libunwindstack.so ..\..\..\Source\libs\ /Y
@REM xcopy .\libunwindstack.a ..\libs\ /Y
@REM xcopy .\deps\android-base\libandroid-base.a ..\libs\ /Y
@REM xcopy .\deps\demangle\libdemangle.a ..\libs\ /Y
@REM xcopy .\deps\liblzma\liblzma.a ..\libs\ /Y
@REM xcopy .\deps\libprocinfo\libprocinfo.a ..\libs\ /Y
@REM xcopy .\deps\sys_compat\libsys_compat.a ..\libs\ /Y
