import bpy
import sys
import importlib

# アドオン開発用のモジュールリロード処理
modules_to_reload = [
    "stretch_vertex",
    "create_ico_sphere",
    "export_scene",
    "my_menu",
    "add_filename",
    "file_name",
    "add_collider",
    "collider",
    "draw_collider",
    "disabled",
    "spawn",
]

for mod_name in modules_to_reload:
    full_mod_name = f"{__package__}.{mod_name}" if __package__ else mod_name
    if full_mod_name in sys.modules:
        try:
            importlib.reload(sys.modules[full_mod_name])
        except Exception:
            pass

from .stretch_vertex import MYADDON_OT_stretch_vertex
from .create_ico_sphere import MYADDON_OT_create_ico_sphere
from .export_scene import MYADDON_OT_export_scene
from .my_menu import TOPBAR_MT_my_menu, MYADDON_OT_remove_menu, draw_my_menu, remove_my_menu
from .add_filename import MYADDON_OT_add_filename
from .file_name import OBJECT_PT_file_name
from .add_collider import MYADDON_OT_add_collider
from .collider import OBJECT_PT_collider
from .draw_collider import DrawCollider
from .disabled import MYADDON_OT_add_disabled, OBJECT_PT_disabled
from .spawn import (
    MYADDON_OT_spawn_import_symbol,
    MYADDON_OT_spawn_create_symbol,
    MYADDON_OT_spawn_create_enemy_symbol,
    MYADDON_OT_spawn_create_player_symbol
)

# 1. アドオン情報
bl_info = {
    "name": "レベルエディタ",
    "author": "Taro Kamata",
    "version": (1, 0),
    "blender": (3, 3, 1),
    "category": "Object"
}

# 2. 登録するクラスのリスト
classes = (
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_export_scene,
    MYADDON_OT_remove_menu,
    TOPBAR_MT_my_menu,
    MYADDON_OT_add_filename,
    OBJECT_PT_file_name,
    MYADDON_OT_add_collider,
    OBJECT_PT_collider,
    MYADDON_OT_add_disabled,
    OBJECT_PT_disabled,
    MYADDON_OT_spawn_import_symbol,
    MYADDON_OT_spawn_create_symbol,
    MYADDON_OT_spawn_create_enemy_symbol,
    MYADDON_OT_spawn_create_player_symbol,
)

def register():
    # 1. 既存のMyMenuをトップバーから全削除（重複防止）
    remove_my_menu()

    # 2. クラス登録
    for cls in classes:
        try:
            bpy.utils.unregister_class(cls)
        except Exception:
            pass
        try:
            bpy.utils.register_class(cls)
        except Exception as e:
            print(f"Failed to register class {cls}: {e}")

    # 3. メニューにMyMenuの描画関数を追加
    bpy.types.TOPBAR_MT_editor_menus.append(draw_my_menu)
    
    # 4. 描画ハンドラの登録
    if DrawCollider.handle:
        try:
            bpy.types.SpaceView3D.draw_handler_remove(DrawCollider.handle, 'WINDOW')
        except Exception:
            pass
        DrawCollider.handle = None
    DrawCollider.handle = bpy.types.SpaceView3D.draw_handler_add(DrawCollider.draw_collider, (), 'WINDOW', 'POST_VIEW')
    
    print("レベルエディタが有効化されました。")

def unregister():
    # 描画ハンドラの解除
    if DrawCollider.handle:
        try:
            bpy.types.SpaceView3D.draw_handler_remove(DrawCollider.handle, 'WINDOW')
        except Exception:
            pass
        DrawCollider.handle = None

    # トップバーからMyMenuを全削除
    remove_my_menu()

    # クラスの登録解除
    for cls in reversed(classes):
        try:
            bpy.utils.unregister_class(cls)
        except Exception:
            pass
            
    print("レベルエディタが無効化されました。")

if __name__ == "__main__":
    try:
        unregister()
    except Exception:
        pass
    register()
