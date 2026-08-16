import addon_utils

# 'level_editor' アドオンをオフ->オンして即座に再登録・リロード
addon_utils.disable("level_editor", default_set=True)
addon_utils.enable("level_editor", default_set=True)
print("レベルエディタ アドオンを即時リロード・再登録しました！")
