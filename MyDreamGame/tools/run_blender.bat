@echo off
chcp 65001 >nul
setlocal

rem スクリプトのディレクトリパスを取得
set "SCRIPT_DIR=%~dp0"
set "BLENDER_PORTABLE=%SCRIPT_DIR%blender\blender.exe"
set "BLENDER_DEFAULT_44=C:\Program Files\Blender Foundation\Blender 4.4\blender.exe"
set "BLENDER_DEFAULT=C:\Program Files\Blender Foundation\Blender\blender.exe"
set "DEFAULT_BLEND=%SCRIPT_DIR%TL.blend"

echo ===================================================
echo  MyDreamGame Blender Launcher
echo ===================================================

rem 1. Blender実行ファイルの決定
if exist "%BLENDER_PORTABLE%" (
    set "BLENDER_EXE=%BLENDER_PORTABLE%"
    echo [INFO] ポータブル版Blenderを使用します: %BLENDER_PORTABLE%
) else if exist "%BLENDER_DEFAULT_44%" (
    set "BLENDER_EXE=%BLENDER_DEFAULT_44%"
    echo [INFO] インストール済みBlender 4.4を使用します: %BLENDER_DEFAULT_44%
) else if exist "%BLENDER_DEFAULT%" (
    set "BLENDER_EXE=%BLENDER_DEFAULT%"
    echo [INFO] インストール済みBlenderを使用します: %BLENDER_DEFAULT%
) else (
    where blender >nul 2>&1
    if %errorlevel% equ 0 (
        set "BLENDER_EXE=blender"
        echo [INFO] PATH上のBlenderを使用します。
    ) else (
        echo [ERROR] Blenderが見つかりませんでした。
        echo tools\blender\ にポータブル版Blenderを配置するか、Blenderをインストールしてください。
        pause
        exit /b 1
    )
)

rem 2. アドオン (level_editor) の同期
set "ADDON_SRC=%SCRIPT_DIR%blender_addons\level_editor"
set "ADDON_DEST=%APPDATA%\Blender Foundation\Blender\4.4\scripts\addons\level_editor"

if exist "%ADDON_SRC%" (
    echo [INFO] level_editor アドオンを同期中...
    if not exist "%ADDON_DEST%" mkdir "%ADDON_DEST%"
    xcopy /E /I /Y /Q "%ADDON_SRC%" "%ADDON_DEST%" >nul
    echo [INFO] アドオンの同期が完了しました。
) else (
    echo [WARNING] アドオンソースが見つかりませんでした: %ADDON_SRC%
)

rem 3. 起動パラメータの決定 (引数が無ければ TL.blend を開く)
set "TARGET_FILE=%*"
if "%~1"=="" (
    if exist "%DEFAULT_BLEND%" (
        set "TARGET_FILE="%DEFAULT_BLEND%""
        echo [INFO] デフォルトのシーンファイルを開きます: TL.blend
    )
)

rem 4. Blenderの起動
echo [INFO] Blenderを起動します...
start "" "%BLENDER_EXE%" %TARGET_FILE%

endlocal
