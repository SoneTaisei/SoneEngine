import bpy
import addon_utils
import sys

# 'level_editor' アドオンのモジュールキャッシュをクリアして再無効化・再有効化
addon_name = "level_editor"

try:
    addon_utils.disable(addon_name, default_set=True)
except Exception as e:
    print(f"Disable warning: {e}")

# sys.modulesからサブモジュールも含めて全削除
for mod in list(sys.modules.keys()):
    if mod == addon_name or mod.startswith(addon_name + "."):
        del sys.modules[mod]

try:
    addon_utils.enable(addon_name, default_set=True)
    print("レベルエディタ アドオンを即時リロード・再登録しました！")
except Exception as e:
    print(f"Enable error: {e}")
