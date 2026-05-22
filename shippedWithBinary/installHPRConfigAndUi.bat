@echo off
setlocal enabledelayedexpansion

set "CONFIG_DIR=%APPDATA%\HPR\HPR_Config"
set "SCRIPT_DIR=%~dp0"

echo ================================================
echo        HPR Configuration Installer
echo ================================================
echo.
echo This script will copy HPR's required config files
echo and UI/assets to the correct locations on your system.
echo.

echo ^>^> Creating config directory if it doesn't exist...
if not exist "%CONFIG_DIR%" mkdir "%CONFIG_DIR%"
echo    Config dir : %CONFIG_DIR%
echo.

:: aliases.csv
if exist "%CONFIG_DIR%\aliases.csv" (
    set /p "confirm_aliases=^>^> aliases.csv already exists. Overwrite? (y/N): "
    if /i "!confirm_aliases!"=="y" (
        copy /y "%SCRIPT_DIR%aliases.csv" "%CONFIG_DIR%\aliases.csv" >nul
        echo    aliases.csv overwritten.
    ) else (
        echo    aliases.csv skipped. Your edits are safe.
    )
) else (
    copy /y "%SCRIPT_DIR%aliases.csv" "%CONFIG_DIR%\aliases.csv" >nul
    echo ^>^> aliases.csv copied successfully.
)

echo.

:: tabAliases.csv
if exist "%CONFIG_DIR%\tabAliases.csv" (
    set /p "confirm_aliases=^>^> tabAliases.csv already exists. Overwrite? (y/N): "
    if /i "!confirm_aliases!"=="y" (
        copy /y "%SCRIPT_DIR%tabAliases.csv" "%CONFIG_DIR%\tabAliases.csv" >nul
        echo    tabAliases.csv overwritten.
    ) else (
        echo    tabAliases.csv skipped. Your edits are safe.
    )
) else (
    copy /y "%SCRIPT_DIR%tabAliases.csv" "%CONFIG_DIR%\tabAliases.csv" >nul
    echo ^>^> alitabAliasesases.csv copied successfully.
)

echo.

:: config.csv
if exist "%CONFIG_DIR%\config.csv" (
    set /p "confirm_config=^>^> config.csv already exists. Overwrite? (y/N): "
    if /i "!confirm_config!"=="y" (
        copy /y "%SCRIPT_DIR%config.csv" "%CONFIG_DIR%\config.csv" >nul
        echo    config.csv overwritten.
    ) else (
        echo    config.csv skipped. Your edits are safe.
    )
) else (
    copy /y "%SCRIPT_DIR%config.csv" "%CONFIG_DIR%\config.csv" >nul
    echo ^>^> config.csv copied successfully.
)

echo.

:: ui-REFERENCEONLY folder — ALWAYS overwrite silently, no prompt
if exist "%CONFIG_DIR%\ui-REFERENCEONLY\" (
    rmdir /S /Q "%CONFIG_DIR%\ui-REFERENCEONLY"
)
xcopy /E /I /Y "%SCRIPT_DIR%ui" "%CONFIG_DIR%\ui-REFERENCEONLY" >nul
echo ^>^> ui-REFERENCEONLY/ updated to latest defaults silently.

echo.

:: ui folder — only copy if doesnt exist, ask if it does
if exist "%CONFIG_DIR%\ui\" (
    set /p "confirm_ui=^>^> ui/ folder already exists. Overwrite? (y/N): "
    if /i "!confirm_ui!"=="y" (
        rmdir /S /Q "%CONFIG_DIR%\ui"
        xcopy /E /I /Y "%SCRIPT_DIR%ui" "%CONFIG_DIR%\ui" >nul
        echo    ui/ overwritten. Your custom UI has been replaced with defaults.
    ) else (
        echo    ui/ skipped. Your custom UI is safe.
        echo    Reference the latest default UI at:
        echo    %CONFIG_DIR%\ui-REFERENCEONLY\
    )
) else (
    xcopy /E /I /Y "%SCRIPT_DIR%ui" "%CONFIG_DIR%\ui" >nul
    echo ^>^> ui/ folder copied successfully.
)

echo.

:: assets folder — ALWAYS replace silently
if exist "%CONFIG_DIR%\assets\" (
    rmdir /S /Q "%CONFIG_DIR%\assets"
)
xcopy /E /I /Y "%SCRIPT_DIR%assets" "%CONFIG_DIR%\assets" >nul
echo ^>^> assets/ folder updated silently.

echo.
echo ================================================
echo   Done! You can now run HPR.
echo.
echo   Your configuration is located at:
echo   %CONFIG_DIR%
echo.
echo   Latest default UI is always available at:
echo   %CONFIG_DIR%\ui-REFERENCEONLY\
echo ================================================

endlocal