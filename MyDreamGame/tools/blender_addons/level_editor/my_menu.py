import bpy

# --- サブメニュークラス ---
class TOPBAR_MT_my_menu(bpy.types.Menu):
    bl_idname = "TOPBAR_MT_my_menu"
    bl_label = "MyMenu"
    bl_description = "拡張メニュー by Taro Kamata"

    def draw(self, context):
        layout = self.layout

        # 頂点を伸ばす
        layout.operator("myaddon.myaddon_ot_stretch_vertex", icon='VERTEXSEL')
        # ICO球生成
        layout.operator("myaddon.myaddon_ot_create_ico_sphere", icon='MESH_ICOSPHERE')
        # シーン出力
        layout.operator("myaddon.myaddon_ot_export_scene", icon='EXPORT')
        
        # 敵出現ポイントおよびプレイヤー出現ポイントの作成メニュー
        layout.operator("myaddon.myaddon_ot_spawn_create_enemy_symbol", text="敵出現ポイントシンボルの作成", icon='OUTLINER_OB_EMPTY')
        layout.operator("myaddon.myaddon_ot_spawn_create_player_symbol", text="プレイヤー出現ポイントシンボルの作成", icon='OUTLINER_OB_EMPTY')
        
        # 区切り線
        layout.separator()
        # マニュアル項目
        layout.operator("wm.url_open_preset", text="Manual", icon='HELP')
        # 区切り線
        layout.separator()
        # メニュー削除ボタン
        layout.operator("myaddon.myaddon_ot_remove_menu", icon='CANCEL')

    def submenu(self, context):
        self.layout.menu(TOPBAR_MT_my_menu.bl_idname)

# --- オペレータ: メニュー削除 ---
class MYADDON_OT_remove_menu(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_remove_menu"
    bl_label = "メニューを削除"
    bl_description = "トップバーからこのメニューを削除します"

    def execute(self, context):
        remove_my_menu()
        self.report({'INFO'}, "MyMenuを削除しました")
        return {'FINISHED'}

def remove_my_menu():
    """既存のMyMenuサブメニューをトップバーから完全に全削除（重複防止）"""
    if hasattr(bpy.types.TOPBAR_MT_editor_menus, "_draw_funcs"):
        draw_funcs = bpy.types.TOPBAR_MT_editor_menus._draw_funcs
        to_remove = []
        for func in draw_funcs:
            name = getattr(func, "__name__", "")
            qualname = getattr(func, "__qualname__", "")
            func_str = str(func)
            
            # submenu, draw_menu_manual, TOPBAR_MT_my_menu 関連の関数をすべて検出
            if (name in ("submenu", "draw_menu_manual") or 
                "TOPBAR_MT_my_menu" in qualname or 
                "TOPBAR_MT_my_menu" in func_str or
                "my_menu" in name.lower()):
                to_remove.append(func)
        
        # 検出した関数を重複含めてすべて削除
        for func in to_remove:
            while func in draw_funcs:
                try:
                    draw_funcs.remove(func)
                except ValueError:
                    break
    else:
        try:
            bpy.types.TOPBAR_MT_editor_menus.remove(TOPBAR_MT_my_menu.submenu)
        except Exception:
            pass
