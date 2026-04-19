@echo off
:: ① バッチファイルがあるフォルダに移動する（これが超重要！）
cd /d %~dp0

:: ドロップされたファイル名を取得
set FILENAME=%~n1

echo 変換を開始します: %FILENAME%

:: ② cmftで変換（.\ を外して直接呼ぶ）
cmftRelease.exe --input "skybox.tga" --outputNum 1 --output0 "temp_cube" --output0params dds,rgba16f,cubemap

:: ③ texconvで圧縮
texconv.exe -f BC6H_UF16 -y "temp_cube.dds"

:: ④ 名前を整えて完成！
if exist "temp_cube.dds" (
    move /y "temp_cube.dds" "%FILENAME%_skybox.dds"
    echo ------------------------------------------
    echo 【成功】 %FILENAME%_skybox.dds ができました！
    echo ------------------------------------------
) else (
    echo 【エラー】 変換に失敗しました。上のメッセージを確認してください。
)

pause