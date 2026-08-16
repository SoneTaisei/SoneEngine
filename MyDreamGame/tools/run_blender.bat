@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "BLENDER_PORTABLE=%SCRIPT_DIR%blender\blender.exe"
set "BLENDER_DEFAULT_44=C:\Program Files\Blender Foundation\Blender 4.4\blender.exe"
set "BLENDER_DEFAULT=C:\Program Files\Blender Foundation\Blender\blender.exe"
set "DEFAULT_BLEND=%SCRIPT_DIR%TL.blend"

echo ===================================================
echo  MyDreamGame Blender Launcher
echo ===================================================

if exist "%BLENDER_PORTABLE%" (
    set "BLENDER_EXE=%BLENDER_PORTABLE%"
    echo [INFO] Found portable Blender: %BLENDER_PORTABLE%
) else if exist "%BLENDER_DEFAULT_44%" (
    set "BLENDER_EXE=%BLENDER_DEFAULT_44%"
    echo [INFO] Found installed Blender 4.4: %BLENDER_DEFAULT_44%
) else if exist "%BLENDER_DEFAULT%" (
    set "BLENDER_EXE=%BLENDER_DEFAULT%"
    echo [INFO] Found installed Blender: %BLENDER_DEFAULT%
) else (
    where blender >nul 2>&1
    if %errorlevel% equ 0 (
        set "BLENDER_EXE=blender"
        echo [INFO] Found Blender in PATH
    ) else (
        echo [ERROR] Blender executable not found!
        echo Please place portable Blender in tools\blender\ or install Blender.
        pause
        exit /b 1
    )
)

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

rem 4. Blenderの起動 (アドオン自動有効化パラメータを追加)
echo [INFO] Blenderを起動します...
start "" "%BLENDER_EXE%" %TARGET_FILE% --python-expr "import addon_utils; addon_utils.enable('level_editor', default_set=True)"

endlocal
