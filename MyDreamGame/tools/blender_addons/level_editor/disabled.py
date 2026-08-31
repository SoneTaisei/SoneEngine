import bpy

# --- オペレータ: 無効オプション追加 ---
class MYADDON_OT_add_disabled(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_add_disabled"
    bl_label = "Add Disabled"
    bl_description = "['無効オプション']カスタムプロパティを追加します"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        # ['無効オプション']カスタムプロパティを追加 (初期値: True)
        context.object["無効オプション"] = True
        return {"FINISHED"}


# --- パネル: 無効オプション ---
class OBJECT_PT_disabled(bpy.types.Panel):
    """オブジェクトの無効オプションパネル"""
    bl_idname = "OBJECT_PT_disabled"
    bl_label = "Disabled"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    def draw(self, context):
        # パネルに項目を追加
        if "無効オプション" in context.object:
            # 既にプロパティがあれば、プロパティを表示
            self.layout.prop(context.object, '["無効オプション"]', text="disabled")
        elif "disabled" in context.object:
            self.layout.prop(context.object, '["disabled"]', text="disabled")
        else:
            # プロパティがなければ、プロパティ追加ボタンを表示
            self.layout.operator(MYADDON_OT_add_disabled.bl_idname)
