import bpy
import os

# --- データテーブル ---
class SpawnNames():
    # インデックス
    PROTOTYPE = 0  # プロトタイプのオブジェクト名
    INSTANCE = 1   # 量産時のオブジェクト名
    FILENAME = 2   # リソースファイル名

    names = {}
    names["Enemy"] = ("PrototypeEnemySpawn", "EnemySpawn", os.path.join("needle", "needle.obj"))
    names["Player"] = ("PrototypePlayerSpawn", "PlayerSpawn", os.path.join("player", "player.obj"))


# --- オペレータ: 出現ポイントのシンボルを読み込む ---
class MYADDON_OT_spawn_import_symbol(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_spawn_import_symbol"
    bl_label = "出現ポイントシンボルImport"
    bl_description = "出現ポイントのシンボルをImportします"
    bl_options = {'REGISTER', 'UNDO'}

    def load_obj(self, type):
        prototype_name = SpawnNames.names[type][SpawnNames.PROTOTYPE]

        # 既存のプロトタイプおよび紐付いている旧メッシュデータを完全に削除して最新モデルで更新
        spawn_object = bpy.data.objects.get(prototype_name)
        if spawn_object is not None:
            mesh_data = spawn_object.data
            bpy.data.objects.remove(spawn_object, do_unlink=True)
            if mesh_data is not None and isinstance(mesh_data, bpy.types.Mesh):
                try:
                    bpy.data.meshes.remove(mesh_data, do_unlink=True)
                except Exception:
                    pass

        # スクリプトが配置されているディレクトリの名前を取得する
        addon_directory = os.path.dirname(__file__)
        relative_path = SpawnNames.names[type][SpawnNames.FILENAME]
        full_path = os.path.join(addon_directory, relative_path)

        if not os.path.exists(full_path):
            fallback_path = os.path.join(addon_directory, "player", "player.obj")
            if os.path.exists(fallback_path):
                full_path = fallback_path
            else:
                self.report({'ERROR'}, f"モデルファイルが見つかりません: {full_path}")
                return {'CANCELLED'}

        # モデルの読み込み
        bpy.ops.wm.obj_import('EXEC_DEFAULT',
            filepath=full_path,
            display_type='THUMBNAIL',
            forward_axis='Z',
            up_axis='Y'
        )

        # 回転の適用
        bpy.ops.object.transform_apply(
            location=False,
            rotation=True,
            scale=False,
            properties=False,
            isolate_users=False
        )

        # インポートしたオブジェクトを取得
        object = bpy.context.active_object
        if object is None and len(bpy.context.selected_objects) > 0:
            object = bpy.context.selected_objects[0]

        if object is not None:
            # オブジェクト名を変更
            object.name = prototype_name
            # カスタムプロパティにオブジェクトの種類を設定
            object["type"] = SpawnNames.names[type][SpawnNames.INSTANCE]

            # メモリ上には置いておくがシーンからは外す
            if object.name in bpy.context.collection.objects:
                bpy.context.collection.objects.unlink(object)

        return {'FINISHED'}

    def execute(self, context):
        print("出現ポイントのシンボルをImportします")
        # Enemyオブジェクト読み込み
        self.load_obj("Enemy")
        # Playerオブジェクト読み込み
        self.load_obj("Player")

        return {'FINISHED'}


# --- オペレータ: 出現ポイントのシンボルを作成・配置 ---
class MYADDON_OT_spawn_create_symbol(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_spawn_create_symbol"
    bl_label = "出現ポイントシンボルの作成"
    bl_description = "出現ポイントのシンボルを作成・配置します"
    bl_options = {'REGISTER', 'UNDO'}

    type: bpy.props.StringProperty(name="Type", default="Player")

    def execute(self, context):
        print(f"出現ポイントのシンボル({self.type})を作成します")

        if self.type not in SpawnNames.names:
            self.report({'ERROR'}, f"不明なタイプです: {self.type}")
            return {'CANCELLED'}

        prototype_name = SpawnNames.names[self.type][SpawnNames.PROTOTYPE]
        instance_name = SpawnNames.names[self.type][SpawnNames.INSTANCE]

        # プロトタイプモデルを常に最新ファイルから強制更新・取得
        bpy.ops.myaddon.myaddon_ot_spawn_import_symbol('EXEC_DEFAULT')
        spawn_object = bpy.data.objects.get(prototype_name)

        if spawn_object is None:
            self.report({'ERROR'}, f"シンボルモデル({self.type})の読み込みに失敗しました")
            return {'CANCELLED'}

        # Blenderでの選択を解除する
        bpy.ops.object.select_all(action='DESELECT')

        # 複製元の非表示オブジェクトを複製する
        object = spawn_object.copy()
        if spawn_object.data:
            object.data = spawn_object.data.copy()

        # 複製したオブジェクトを現在のシーンにリンク（出現させる）
        bpy.context.collection.objects.link(object)

        # オブジェクト名を変更
        object.name = instance_name

        # 作成したオブジェクトを選択状態・アクティブにする
        object.select_set(True)
        bpy.context.view_layer.objects.active = object

        return {'FINISHED'}


# --- 敵専用オペレータ ---
class MYADDON_OT_spawn_create_enemy_symbol(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_spawn_create_enemy_symbol"
    bl_label = "敵出現ポイントシンボルの作成"
    bl_description = "敵出現ポイントのシンボルを作成します"

    def execute(self, context):
        bpy.ops.myaddon.myaddon_ot_spawn_create_symbol('EXEC_DEFAULT', type="Enemy")
        return {'FINISHED'}


# --- 自キャラ専用オペレータ ---
class MYADDON_OT_spawn_create_player_symbol(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_spawn_create_player_symbol"
    bl_label = "プレイヤー出現ポイントシンボルの作成"
    bl_description = "プレイヤー出現ポイントのシンボルを作成します"

    def execute(self, context):
        bpy.ops.myaddon.myaddon_ot_spawn_create_symbol('EXEC_DEFAULT', type="Player")
        return {'FINISHED'}
