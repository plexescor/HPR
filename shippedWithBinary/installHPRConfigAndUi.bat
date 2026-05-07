@echo off
setlocal

set "CONFIG_DIR=%APPDATA%\HPR\HPR_Config"
set "SCRIPT_DIR=%~dp0"
set "FORCE=false"

if "%1"=="--force" (
    set "FORCE=true"
    echo ================================================
    echo        HPR Configuration Installer
    echo ================================================
    echo.
    echo !! WARNING: --force is set. This will overwrite your
    echo    existing config files and UI assets with the defaults.
    echo    Any changes YOU made to aliases.csv, config.csv,
    echo    or the ui/ folder will be permanently lost!
    echo.
    set /p "confirm=   Are you sure? (y/N): "
    if /i not "%confirm%"=="y" (
        echo    Aborted. Your configuration is untouched.
        exit /b 0
    )
    echo.
) else (
    echo ================================================
    echo        HPR Configuration Installer
    echo ================================================
    echo.
    echo This script will copy HPR's required config files
    echo and UI assets to the correct locations on your system.
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

if exist "%CONFIG_DIR%\ui\" (
    if "%FORCE%"=="false" (
        echo ^>^> ui/ folder already exists -- skipping.
        echo    ^(run with --force to overwrite UI assets!^)
    ) else (
        xcopy /E /I /Y "%SCRIPT_DIR%ui" "%CONFIG_DIR%\ui" >nul
        echo ^>^> ui/ folder copied successfully.
    )
) else (
    xcopy /E /I /Y "%SCRIPT_DIR%ui" "%CONFIG_DIR%\ui" >nul
    echo ^>^> ui/ folder copied successfully.
)

echo.
echo ================================================
echo   Done! You can now run HPR.
echo.
echo   Your configuration is located at:
echo   %CONFIG_DIR%
echo.
echo   To reset config and UI to defaults, run:
echo   copyHPRConfig.bat --force
echo   (WARNING: this will overwrite your edits!)
echo ================================================

endlocal