@echo off
setlocal

set "CONFIG_DIR=%APPDATA%\HPR"
set "SCRIPT_DIR=%~dp0"
set "FORCE=false"

if "%1"=="--force" (
    set "FORCE=true"
    echo ================================================
    echo        HPR Configuration Installer
    echo ================================================
    echo.
    echo !! WARNING: --force is set. This will overwrite your
    echo    existing config files with the default shipped files.
    echo    Any changes YOU made to aliases.csv or config.csv
    echo    will be permanently lost!
    echo.
    set /p "confirm=   Are you sure? (y/N): "
    if /i not "%confirm%"=="y" (
        echo    Aborted. Your config files are untouched.
        exit /b 0
    )
    echo.
) else (
    echo ================================================
    echo        HPR Configuration Installer
    echo ================================================
    echo.
    echo This script will copy HPR's required config files
    echo to the correct locations on your system.
    echo.
)

echo ^>^> Creating config directory if it doesn't exist...
if not exist "%CONFIG_DIR%" mkdir "%CONFIG_DIR%"
echo    Config dir : %CONFIG_DIR%
echo.

if exist "%CONFIG_DIR%\aliases.csv" (
    if "%FORCE%"=="false" (
        echo ^>^> aliases.csv already exists -- skipping.
        echo    ^(run with --force to overwrite, but you will lose your edits!^)
    ) else (
        copy /y "%SCRIPT_DIR%aliases.csv" "%CONFIG_DIR%\aliases.csv" >nul
        echo ^>^> aliases.csv copied successfully.
    )
) else (
    copy /y "%SCRIPT_DIR%aliases.csv" "%CONFIG_DIR%\aliases.csv" >nul
    echo ^>^> aliases.csv copied successfully.
)

echo.

if exist "%CONFIG_DIR%\config.csv" (
    if "%FORCE%"=="false" (
        echo ^>^> config.csv already exists -- skipping.
        echo    ^(run with --force to overwrite, but you will lose your edits!^)
    ) else (
        copy /y "%SCRIPT_DIR%config.csv" "%CONFIG_DIR%\config.csv" >nul
        echo ^>^> config.csv copied successfully.
    )
) else (
    copy /y "%SCRIPT_DIR%config.csv" "%CONFIG_DIR%\config.csv" >nul
    echo ^>^> config.csv copied successfully.
)

echo.
echo ================================================
echo   Done! You can now run HPR.
echo.
echo   Your config files are located at:
echo   %CONFIG_DIR%
echo.
echo   To reset config to defaults, run:
echo   copyHPRConfig.bat --force
echo   (WARNING: this will overwrite your edits!)
echo ================================================

endlocal