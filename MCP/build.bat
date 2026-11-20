@echo off
setlocal

set MCP_DIR=%~dp0
set BUILD_DIR=%MCP_DIR%build

if "%1"=="clean" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    echo Done.
    exit /b 0
)

echo === Building MemAnalyzer MCP ===
echo.

if not exist "%BUILD_DIR%" (
    echo [1/3] Configuring CMake...
    cmake -S "%MCP_DIR%" -B "%BUILD_DIR%" -A x64
    if errorlevel 1 (
        echo CMake configuration failed!
        exit /b 1
    )
) else (
    echo [1/3] Build directory exists, skipping configure.
)

echo [2/3] Building Release...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo [3/3] Verifying outputs...
set EXPORTER=%BUILD_DIR%\exporter\Release\exporter.exe
set PYD=%BUILD_DIR%\bindings\Release\TraceAnalyzer_py.cp310-win_amd64.pyd

if exist "%EXPORTER%" (
    echo   OK: exporter.exe
) else (
    echo   MISSING: exporter.exe
    exit /b 1
)

if exist "%PYD%" (
    echo   OK: TraceAnalyzer_py.pyd
) else (
    echo   MISSING: TraceAnalyzer_py.pyd
    exit /b 1
)

echo.
echo === Build successful ===
echo Exporter: %EXPORTER%
echo Python module: %PYD%
echo.
echo To start the MCP server:
echo   python "%MCP_DIR%server\server.py"
