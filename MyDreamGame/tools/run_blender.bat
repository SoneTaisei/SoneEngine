@echo off
setlocal enableextensions enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "BLENDER_PORTABLE=%SCRIPT_DIR%blender\blender.exe"
set "DEFAULT_BLEND=%SCRIPT_DIR%TL.blend"

echo ===================================================
echo  MyDreamGame Blender Launcher
echo ===================================================

set "BLENDER_EXE="

rem 1. Check Portable Blender in tools\blender\
if exist "%BLENDER_PORTABLE%" (
    set "BLENDER_EXE=%BLENDER_PORTABLE%"
    echo [INFO] Found portable Blender: %BLENDER_PORTABLE%
    goto FOUND_BLENDER
)

rem 2. Check Windows Registry for installed Blender
for /f "tokens=2*" %%A in ('reg query "HKEY_LOCAL_MACHINE\SOFTWARE\BlenderFoundation" /s /v "InstallDir" 2^>nul') do (
    if exist "%%B\blender.exe" (
        set "BLENDER_EXE=%%B\blender.exe"
        echo [INFO] Found Blender via Registry: !BLENDER_EXE!
        goto FOUND_BLENDER
    )
)

rem 3. Check Common Program Files installation paths
for %%V in (4.4 4.3 4.2 4.1 4.0 3.6 3.5 3.4 3.3) do (
    if exist "C:\Program Files\Blender Foundation\Blender %%V\blender.exe" (
        set "BLENDER_EXE=C:\Program Files\Blender Foundation\Blender %%V\blender.exe"
        echo [INFO] Found Blender %%V: !BLENDER_EXE!
        goto FOUND_BLENDER
    )
)

if exist "C:\Program Files\Blender Foundation\Blender\blender.exe" (
    set "BLENDER_EXE=C:\Program Files\Blender Foundation\Blender\blender.exe"
    echo [INFO] Found default Blender: !BLENDER_EXE!
    goto FOUND_BLENDER
)

rem 4. Check system PATH
where blender >nul 2>&1
if %errorlevel% equ 0 (
    set "BLENDER_EXE=blender"
    echo [INFO] Found Blender in PATH
    goto FOUND_BLENDER
)

:FOUND_BLENDER
if "%BLENDER_EXE%"=="" (
    echo [ERROR] Blender executable could not be found!
    echo Please install Blender or place portable Blender in tools\blender\blender.exe
    pause
    exit /b 1
)

rem Sync level_editor Addon
set "ADDON_SRC=%SCRIPT_DIR%blender_addons\level_editor"
set "ADDON_DEST=%APPDATA%\Blender Foundation\Blender\4.4\scripts\addons\level_editor"

if exist "%ADDON_SRC%" (
    echo [INFO] Syncing level_editor addon...
    if not exist "%ADDON_DEST%" mkdir "%ADDON_DEST%"
    xcopy "%ADDON_SRC%" "%ADDON_DEST%" /E /I /Y /Q >nul
    echo [INFO] Addon sync completed.
) else (
    echo [WARNING] Addon source not found: %ADDON_SRC%
)

set "TARGET_FILE=%*"
if "%~1"=="" (
    if exist "%DEFAULT_BLEND%" (
        set "TARGET_FILE="%DEFAULT_BLEND%""
        echo [INFO] Opening default scene: TL.blend
    )
)

echo [INFO] Launching Blender...
if defined TARGET_FILE (
    start "" "%BLENDER_EXE%" %TARGET_FILE% --python-expr "import addon_utils; addon_utils.enable('level_editor')"
) else (
    start "" "%BLENDER_EXE%" --python-expr "import addon_utils; addon_utils.enable('level_editor')"
)

endlocal
