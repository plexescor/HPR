@echo off
setlocal EnableDelayedExpansion

:: =============================================================================
:: installDependencies.bat
:: Downloads Slint v1.16.1 Windows artifacts into .\external\, extracts them,
:: runs the C++ SDK installer, and copies:
::   slint-lsp    -> C:\Program Files\slint-lsp\
::   slint-viewer -> C:\Program Files\slint-viewer\
::
:: Must be run as Administrator.
:: =============================================================================

set SLINT_VERSION=1.16.1
set SCRIPT_DIR=%~dp0
set EXTERNAL_DIR=%SCRIPT_DIR%external
set LSP_INSTALL_DIR=C:\Program Files\slint-lsp
set VIEWER_INSTALL_DIR=C:\Program Files\slint-viewer

set LSP_URL=https://github.com/slint-ui/slint/releases/download/v%SLINT_VERSION%/slint-lsp-windows-x86_64.zip
set VIEWER_URL=https://github.com/slint-ui/slint/releases/download/v%SLINT_VERSION%/slint-viewer-windows-x86_64.zip

set LSP_FILE=%EXTERNAL_DIR%\slint-lsp-windows-x86_64.zip
set VIEWER_FILE=%EXTERNAL_DIR%\slint-viewer-windows-x86_64.zip

set LSP_EXTRACT_DIR=%EXTERNAL_DIR%\slint-lsp
set VIEWER_EXTRACT_DIR=%EXTERNAL_DIR%\slint-viewer

:: ---------------------------------------------------------------------------
:: Check for Administrator privileges
:: ---------------------------------------------------------------------------
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] This script must be run as Administrator.
    echo         Right-click installDependencies.bat and select "Run as administrator".
    pause
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: Check for required tools
:: ---------------------------------------------------------------------------
where curl >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] curl is not available. Please install curl or update Windows 10 ^(build 1803+^).
    pause
    exit /b 1
)

where tar >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] tar is not available. Please update to Windows 10 build 17063 or later.
    pause
    exit /b 1
)

where cargo >nul 2>&1
if %errorlevel% neq 0 (
    echo [WARN]  cargo (Rust toolchain) is not found in PATH.
    echo         Slint will be compiled from source during CMake configuration.
    echo         Please install Rust from https://rustup.rs if build fails.
)

:: PowerShell is used for Expand-Archive as a fallback check
where powershell >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] PowerShell is not available. This script requires PowerShell.
    pause
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: Step 1 - Prepare external\ directory
:: ---------------------------------------------------------------------------
echo.
echo [INFO]  Preparing external directory: %EXTERNAL_DIR%
if not exist "%EXTERNAL_DIR%" mkdir "%EXTERNAL_DIR%"

:: ---------------------------------------------------------------------------
:: Step 2 - Download archives
:: ---------------------------------------------------------------------------
echo.
echo [INFO]  Downloading Slint v%SLINT_VERSION% release artifacts...

call :download "%LSP_URL%" "%LSP_FILE%"  "slint-lsp zip"
call :download "%VIEWER_URL%" "%VIEWER_FILE%" "slint-viewer zip"

:: ---------------------------------------------------------------------------
:: Step 3 - Extract ZIP archives into external\
:: ---------------------------------------------------------------------------
echo.
echo [INFO]  Extracting ZIP archives...

:: -- slint-lsp ---------------------------------------------------------------
if exist "%LSP_EXTRACT_DIR%" (
    echo [WARN]  Already extracted, skipping: %LSP_EXTRACT_DIR%
) else (
    echo [INFO]  Extracting slint-lsp...
    mkdir "%LSP_EXTRACT_DIR%"
    tar -xf "%LSP_FILE%" -C "%LSP_EXTRACT_DIR%"
    if !errorlevel! neq 0 (
        echo [WARN]  tar failed, trying PowerShell Expand-Archive...
        powershell -NoProfile -Command "Expand-Archive -Path '%LSP_FILE%' -DestinationPath '%LSP_EXTRACT_DIR%' -Force"
    )
    echo [OK]    Extracted to: %LSP_EXTRACT_DIR%
)

:: -- slint-viewer ------------------------------------------------------------
if exist "%VIEWER_EXTRACT_DIR%" (
    echo [WARN]  Already extracted, skipping: %VIEWER_EXTRACT_DIR%
) else (
    echo [INFO]  Extracting slint-viewer...
    mkdir "%VIEWER_EXTRACT_DIR%"
    tar -xf "%VIEWER_FILE%" -C "%VIEWER_EXTRACT_DIR%"
    if !errorlevel! neq 0 (
        echo [WARN]  tar failed, trying PowerShell Expand-Archive...
        powershell -NoProfile -Command "Expand-Archive -Path '%VIEWER_FILE%' -DestinationPath '%VIEWER_EXTRACT_DIR%' -Force"
    )
    echo [OK]    Extracted to: %VIEWER_EXTRACT_DIR%
)

:: ---------------------------------------------------------------------------
:: Step 5 - Install slint-lsp -> C:\Program Files\slint-lsp\
:: ---------------------------------------------------------------------------
echo.
echo [INFO]  Installing slint-lsp to "%LSP_INSTALL_DIR%"...

if not exist "%LSP_INSTALL_DIR%" (
    mkdir "%LSP_INSTALL_DIR%"
)

set LSP_BIN=
for /r "%LSP_EXTRACT_DIR%" %%F in (slint-lsp.exe) do (
    if "!LSP_BIN!"=="" set LSP_BIN=%%F
)

if "!LSP_BIN!"=="" (
    echo [ERROR] Could not locate slint-lsp.exe inside %LSP_EXTRACT_DIR%
    pause
    exit /b 1
)

echo [INFO]  Copying slint-lsp.exe -> "%LSP_INSTALL_DIR%\slint-lsp.exe"
copy /Y "!LSP_BIN!" "%LSP_INSTALL_DIR%\slint-lsp.exe" >nul
echo [OK]    slint-lsp.exe installed.

:: ---------------------------------------------------------------------------
:: Step 6 - Install slint-viewer -> C:\Program Files\slint-viewer\
:: ---------------------------------------------------------------------------
echo.
echo [INFO]  Installing slint-viewer to "%VIEWER_INSTALL_DIR%"...

if not exist "%VIEWER_INSTALL_DIR%" (
    mkdir "%VIEWER_INSTALL_DIR%"
)

set VIEWER_BIN=
for /r "%VIEWER_EXTRACT_DIR%" %%F in (slint-viewer.exe) do (
    if "!VIEWER_BIN!"=="" set VIEWER_BIN=%%F
)

if "!VIEWER_BIN!"=="" (
    echo [ERROR] Could not locate slint-viewer.exe inside %VIEWER_EXTRACT_DIR%
    pause
    exit /b 1
)

echo [INFO]  Copying slint-viewer.exe -> "%VIEWER_INSTALL_DIR%\slint-viewer.exe"
copy /Y "!VIEWER_BIN!" "%VIEWER_INSTALL_DIR%\slint-viewer.exe" >nul
echo [OK]    slint-viewer.exe installed.

:: ---------------------------------------------------------------------------
:: Step 7 - Add both install dirs to the system PATH
:: ---------------------------------------------------------------------------
echo.
echo [INFO]  Updating system PATH...
powershell -NoProfile -Command ^
    "$p = [Environment]::GetEnvironmentVariable('Path','Machine');" ^
    "$dirs = @('%LSP_INSTALL_DIR%', '%VIEWER_INSTALL_DIR%');" ^
    "foreach ($d in $dirs) {" ^
    "    if ($p -notlike ('*' + $d + '*')) {" ^
    "        $p = $p + ';' + $d;" ^
    "        Write-Host ('[OK]    Added to PATH: ' + $d);" ^
    "    } else { Write-Host ('[WARN]  Already in PATH: ' + $d); }" ^
    "} [Environment]::SetEnvironmentVariable('Path', $p, 'Machine');"

:: ---------------------------------------------------------------------------
:: Done
:: ---------------------------------------------------------------------------
echo.
echo =========================================
echo   Slint v%SLINT_VERSION% installed successfully!
echo =========================================
echo.
echo   SDK installer  : ran from %SDK_FILE%
echo   slint-lsp      : %LSP_INSTALL_DIR%\slint-lsp.exe
echo   slint-viewer   : %VIEWER_INSTALL_DIR%\slint-viewer.exe
echo.
echo   Open a new terminal and run:
echo     slint-lsp --version
echo     slint-viewer --version
echo.
pause
exit /b 0

:: =============================================================================
:: :download  <url>  <dest_file>  <label>
:: =============================================================================
:download
set _URL=%~1
set _DEST=%~2
set _LABEL=%~3

if exist "%_DEST%" (
    echo [WARN]  Already exists, skipping download: %_LABEL%
    goto :eof
)

echo [INFO]  Downloading %_LABEL%...
curl -fL --progress-bar -o "%_DEST%" "%_URL%"
if %errorlevel% neq 0 (
    echo [ERROR] Download failed for: %_URL%
    pause
    exit /b 1
)
echo [OK]    Saved: %_DEST%
goto :eof
