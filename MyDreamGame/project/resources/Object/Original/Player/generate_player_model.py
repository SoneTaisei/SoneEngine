import bpy
import math
import zlib
import struct
import os

# 1. 8x8カラーパレットテクスチャ画像の生成
def save_png(width, height, pixels, filepath):
    png = b'\x89PNG\r\n\x1a\n'
    ihdr_data = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    png += struct.pack('>I', 13) + b'IHDR' + ihdr_data + struct.pack('>I', zlib.crc32(b'IHDR' + ihdr_data))
    
    raw_data = b''
    for y in range(height):
        raw_data += b'\x00'
        for x in range(width):
            r, g, b = pixels[y * width + x]
            raw_data += bytes([r, g, b])
            
    idat_data = zlib.compress(raw_data)
    png += struct.pack('>I', len(idat_data)) + b'IDAT' + idat_data + struct.pack('>I', zlib.crc32(b'IDAT' + idat_data))
    
    png += struct.pack('>I', 0) + b'IEND' + struct.pack('>I', zlib.crc32(b'IEND'))
    
    with open(filepath, 'wb') as f:
        f.write(png)

# 8x8 カラーパレットの定義（2行構成：0行目ベース、1行目影/アクセント）
# X軸：0:肌, 1:髪赤, 2:服青, 3:ズボン黒/紺, 4:靴茶, 5:白目/金属グレー, 6:瞳茶/木シャフト, 7:リュック茶
colors = [
    # Y = 0 (Base Colors)
    (255, 204, 178),  # 0: Skin Peach
    (230, 55, 35),    # 1: Hair Red
    (70, 140, 200),   # 2: Clothes Blue (Puffer Coat)
    (35, 35, 45),     # 3: Pants Black/Navy
    (125, 75, 45),    # 4: Shoes Brown
    (255, 255, 255),  # 5: White
    (75, 45, 30),     # 6: Eye Brown / Wood
    (155, 95, 55),    # 7: Backpack Brown
    
    # Y = 1 (Shadow & Accent Colors)
    (220, 160, 140),  # 0: Skin Shadow
    (160, 30, 20),    # 1: Hair Shadow
    (45, 95, 145),    # 2: Clothes Shadow
    (20, 20, 25),     # 3: Pants Shadow
    (230, 80, 20),    # 4: Boot Cuff Orange (折り返しソックス)
    (180, 185, 190),  # 5: Metal Grey (ピッケル金属刃)
    (110, 60, 30),    # 6: Dark Wood / Eye Shadow
    (105, 60, 30)     # 7: Backpack Strap Dark Brown
]
pixels = colors + [(0, 0, 0)] * 48
tex_filename = "Player_Diffuse.png"
save_png(8, 8, pixels, tex_filename)

# 2. Blenderモデリング
bpy.ops.wm.read_factory_settings(use_empty=True)

def set_uv_to_color(obj, color_index, use_shadow=False):
    mesh = obj.data
    if not mesh.uv_layers:
        mesh.uv_layers.new()
    uv_layer = mesh.uv_layers.active.data
    u = (color_index + 0.5) / 8.0
    v = (1.5 if use_shadow else 0.5) / 8.0
    for loop in uv_layer:
        loop.uv = (u, v)

# --- 顎ラインを持たせた高精度頭部作成 ---
def create_head(name, radius, location, color_index=0):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=radius, location=location, segments=32, ring_count=32)
    obj = bpy.context.active_object
    obj.name = name
    
    bpy.ops.object.mode_set(mode='EDIT')
    import bmesh
    bm = bmesh.from_edit_mesh(obj.data)
    for v in bm.verts:
        # 下半球をシャープにしてあごを作る
        if v.co.z < 0:
            factor = 1.0 + (v.co.z / radius) * 0.35
            v.co.x *= factor
            v.co.y *= (factor * 1.05)
            v.co.y -= (v.co.z / radius) * 0.04
    bmesh.update_edit_mesh(obj.data)
    bpy.ops.object.mode_set(mode='OBJECT')
    
    set_uv_to_color(obj, color_index)
    return obj

# --- テーパー付き円柱 ---
def create_tapered_cylinder(name, radius_bottom, radius_top, height, location, rotation=(0,0,0), color_index=0, use_shadow=False):
    bpy.ops.mesh.primitive_cylinder_add(radius=radius_bottom, depth=height, location=location, vertices=32)
    obj = bpy.context.active_object
    obj.name = name
    
    bpy.ops.object.mode_set(mode='EDIT')
    import bmesh
    bm = bmesh.from_edit_mesh(obj.data)
    half_h = height / 2.0
    for v in bm.verts:
        ratio = (v.co.z + half_h) / height
        scale_factor = (1.0 - ratio) + ratio * (radius_top / radius_bottom)
        v.co.x *= scale_factor
        v.co.y *= scale_factor
    bmesh.update_edit_mesh(obj.data)
    bpy.ops.object.mode_set(mode='OBJECT')
    
    obj.rotation_euler = rotation
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    
    set_uv_to_color(obj, color_index, use_shadow)
    return obj

# --- つま先が丸く偏平した靴 ---
def create_shoe(name, size, location, color_index=4):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.active_object
    obj.name = name
    
    obj.scale = size
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    
    bpy.ops.object.mode_set(mode='EDIT')
    import bmesh
    bm = bmesh.from_edit_mesh(obj.data)
    for v in bm.verts:
        if v.co.y < 0: # つま先側
            v.co.z *= (1.0 + v.co.y * 0.75)
            v.co.x *= (1.0 + v.co.y * 0.3)
        else: # かかと側
            v.co.x *= (1.0 - v.co.y * 0.15)
    bmesh.update_edit_mesh(obj.data)
    bpy.ops.object.mode_set(mode='OBJECT')
    
    set_uv_to_color(obj, color_index)
    return obj

def create_generic_part(name, shape_type, scale, location, color_index, rotation=(0,0,0), use_shadow=False):
    if shape_type == 'sphere':
        bpy.ops.mesh.primitive_uv_sphere_add(radius=1.0, location=location, segments=32, ring_count=32)
    elif shape_type == 'cube':
        bpy.ops.mesh.primitive_cube_add(size=2.0, location=location)
    
    obj = bpy.context.active_object
    obj.name = name
    
    obj.scale = scale
    obj.rotation_euler = rotation
    
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    
    set_uv_to_color(obj, color_index, use_shadow)
    return obj

parts = []

# --- 頭部と顔の表情（画像参照） ---
parts.append(create_head('Head', 0.16, (0.0, 0.0, 0.7), 0))
# 耳 (左右に平たい球体)
parts.append(create_generic_part('Ear_L', 'sphere', (0.025, 0.015, 0.04), (-0.17, 0.01, 0.7), 0))
parts.append(create_generic_part('Ear_R', 'sphere', (0.025, 0.015, 0.04), (0.17, 0.01, 0.7), 0))
# 鼻 (真ん中に小さく配置)
parts.append(create_generic_part('Nose', 'sphere', (0.015, 0.015, 0.015), (0.0, -0.16, 0.69), 0))
# 目 (画像に合わせた大きな茶色の瞳)
parts.append(create_generic_part('WhiteEye_L', 'cube', (0.02, 0.005, 0.025), (-0.05, -0.155, 0.72), 5))
parts.append(create_generic_part('WhiteEye_R', 'cube', (0.02, 0.005, 0.025), (0.05, -0.155, 0.72), 5))
parts.append(create_generic_part('Iris_L', 'cube', (0.012, 0.005, 0.02), (-0.045, -0.160, 0.72), 6)) # 茶色の瞳
parts.append(create_generic_part('Iris_R', 'cube', (0.012, 0.005, 0.02), (0.045, -0.160, 0.72), 6)) # 茶色の瞳
# 眉毛
parts.append(create_generic_part('Eyebrow_L', 'cube', (0.018, 0.004, 0.006), (-0.05, -0.16, 0.765), 6, (0, 0.1, 0.1)))
parts.append(create_generic_part('Eyebrow_R', 'cube', (0.018, 0.004, 0.006), (0.05, -0.16, 0.765), 6, (0, -0.1, -0.1)))
# 頬の赤み (画像にあるピンク色のチーク)
parts.append(create_generic_part('Cheek_L', 'cube', (0.018, 0.004, 0.01), (-0.08, -0.155, 0.66), 1, (0,0,0)))
parts.append(create_generic_part('Cheek_R', 'cube', (0.018, 0.004, 0.01), (0.08, -0.155, 0.66), 1, (0,0,0)))

# --- 髪の毛 (Hair) ---
# アホ毛 (頭頂部からカールして伸びる一本毛)
parts.append(create_tapered_cylinder('Ahoge_1', 0.012, 0.008, 0.06, (0.0, -0.02, 0.88), (0.3, 0.0, 0.0), 1))
parts.append(create_tapered_cylinder('Ahoge_2', 0.008, 0.004, 0.05, (0.0, 0.02, 0.93), (0.6, 0.0, 0.0), 1))
parts.append(create_tapered_cylinder('Ahoge_3', 0.004, 0.001, 0.04, (0.0, 0.07, 0.96), (1.1, 0.0, 0.0), 1))
# 前髪
parts.append(create_generic_part('Bangs', 'cube', (0.12, 0.05, 0.04), (0.0, -0.10, 0.81), 1))
# 横髪
parts.append(create_tapered_cylinder('SideHair_L', 0.03, 0.01, 0.16, (-0.15, -0.05, 0.62), (0.1, 0, -0.2), 1))
parts.append(create_tapered_cylinder('SideHair_R', 0.03, 0.01, 0.16, (0.15, -0.05, 0.62), (0.1, 0, 0.2), 1))
# なびくボリュームのある後ろ髪 (赤色、陰影のためのテーパーシリンダー)
parts.append(create_generic_part('Hair_1', 'sphere', (0.14, 0.14, 0.14), (0.0, 0.12, 0.68), 1))
parts.append(create_tapered_cylinder('Hair_2', 0.12, 0.10, 0.24, (0.0, 0.22, 0.58), (0.3, 0, 0), 1))
parts.append(create_tapered_cylinder('Hair_3', 0.10, 0.08, 0.20, (0.0, 0.30, 0.46), (0.4, 0, 0), 1, use_shadow=True))
parts.append(create_tapered_cylinder('Hair_4', 0.08, 0.04, 0.16, (0.0, 0.36, 0.34), (0.5, 0, 0), 1, use_shadow=True))

# --- 服装（もこもこダウンジャケット & 立ち襟） ---
# 立ち襟 (Collar)
parts.append(create_tapered_cylinder('Collar', 0.05, 0.07, 0.06, (0.0, 0.0, 0.58), (0,0,0), 2))
# キルティング胴体 (平たい球体を4段重ねてもこもこ感を演出)
parts.append(create_generic_part('Coat_Body_1', 'sphere', (0.13, 0.11, 0.045), (0.0, 0.0, 0.52), 2))
parts.append(create_generic_part('Coat_Body_2', 'sphere', (0.14, 0.12, 0.045), (0.0, 0.0, 0.44), 2))
parts.append(create_generic_part('Coat_Body_3', 'sphere', (0.14, 0.12, 0.045), (0.0, 0.0, 0.36), 2))
parts.append(create_generic_part('Coat_Body_4', 'sphere', (0.13, 0.11, 0.045), (0.0, 0.0, 0.28), 2, use_shadow=True)) # 裾側は影色
# フード
parts.append(create_generic_part('Hood', 'sphere', (0.09, 0.07, 0.09), (0.0, 0.11, 0.48), 2, use_shadow=True))
# フード紐
parts.append(create_tapered_cylinder('String_L', 0.008, 0.004, 0.16, (-0.04, -0.11, 0.40), (0.2, 0.1, 0), 7))
parts.append(create_tapered_cylinder('String_R', 0.008, 0.004, 0.16, (0.04, -0.11, 0.40), (0.2, -0.1, 0), 7))

# もこもこ腕 (球体を横方向に4段並べてキルティングの腕を表現)
# 左腕
parts.append(create_generic_part('Arm_L1', 'sphere', (0.045, 0.045, 0.045), (-0.14, 0.0, 0.45), 2))
parts.append(create_generic_part('Arm_L2', 'sphere', (0.042, 0.042, 0.042), (-0.19, 0.0, 0.45), 2))
parts.append(create_generic_part('Arm_L3', 'sphere', (0.038, 0.038, 0.038), (-0.24, 0.0, 0.45), 2))
parts.append(create_generic_part('Arm_L4', 'sphere', (0.034, 0.034, 0.034), (-0.29, 0.0, 0.45), 2))
# 右腕
parts.append(create_generic_part('Arm_R1', 'sphere', (0.045, 0.045, 0.045), (0.14, 0.0, 0.45), 2))
parts.append(create_generic_part('Arm_R2', 'sphere', (0.042, 0.042, 0.042), (0.19, 0.0, 0.45), 2))
parts.append(create_generic_part('Arm_R3', 'sphere', (0.038, 0.038, 0.038), (0.24, 0.0, 0.45), 2))
parts.append(create_generic_part('Arm_R4', 'sphere', (0.034, 0.034, 0.034), (0.29, 0.0, 0.45), 2))

# 手 (肌色)
parts.append(create_generic_part('Hand_L', 'sphere', (0.026, 0.026, 0.026), (-0.33, 0.0, 0.45), 0))
parts.append(create_generic_part('Hand_R', 'sphere', (0.026, 0.026, 0.026), (0.33, 0.0, 0.45), 0))

# --- 背中の装備 (リュック & ピッケル) ---
# バックパック (Backpack) - 茶色
parts.append(create_generic_part('Backpack', 'cube', (0.08, 0.06, 0.10), (0.0, 0.13, 0.40), 7))
# 肩紐 (左右に回す)
parts.append(create_generic_part('Strap_L', 'cube', (0.015, 0.015, 0.08), (-0.08, -0.09, 0.42), 7, (0.2, 0, 0), use_shadow=True)) # 濃い茶色
parts.append(create_generic_part('Strap_R', 'cube', (0.015, 0.015, 0.08), (0.08, -0.09, 0.42), 7, (0.2, 0, 0), use_shadow=True)) # 濃い茶色
# ピッケル (銀色刃と木製シャフト) - リュックの左側に配置
parts.append(create_tapered_cylinder('Pikkel_Shaft', 0.007, 0.007, 0.16, (-0.11, 0.11, 0.28), (0.1, 0.2, 0.5), 6, use_shadow=True)) # 木製
parts.append(create_generic_part('Pikkel_Blade', 'cube', (0.04, 0.008, 0.01), (-0.11, 0.11, 0.36), 5, (0.1, 0.2, 0.5), use_shadow=True)) # 金属

# --- 下半身と靴（オレンジのリブソックス） ---
# 脚 (ズボン)
parts.append(create_tapered_cylinder('Leg_L', 0.045, 0.035, 0.16, (-0.06, 0.0, 0.24), (0, 0, 0), 3))
parts.append(create_tapered_cylinder('Leg_R', 0.045, 0.035, 0.16, (0.06, 0.0, 0.24), (0, 0, 0), 3))

# ブーツのオレンジリブ (ソックスの折り返し)
parts.append(create_generic_part('BootCuff_L', 'sphere', (0.048, 0.048, 0.02), (-0.06, -0.02, 0.16), 4, (0,0,0), use_shadow=True)) # オレンジ
parts.append(create_generic_part('BootCuff_R', 'sphere', (0.048, 0.048, 0.02), (0.06, -0.02, 0.16), 4, (0,0,0), use_shadow=True)) # オレンジ

# 靴 (登山ブーツ)
parts.append(create_shoe('Shoe_L', (0.048, 0.065, 0.03), (-0.06, -0.03, 0.12), 4))
parts.append(create_shoe('Shoe_R', (0.048, 0.065, 0.03), (0.06, -0.03, 0.12), 4))

# 結合処理
bpy.ops.object.select_all(action='DESELECT')
for obj in parts:
    obj.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()

merged_obj = bpy.context.active_object
merged_obj.name = "PlayerModel"

# マテリアルの設定
mat = bpy.data.materials.new(name="PlayerMaterial")
mat.use_nodes = True
nodes = mat.node_tree.nodes
links = mat.node_tree.links

bsdf = nodes.get("Principled BSDF") or next(n for n in nodes if n.type == 'BSDF_PRINCIPLED')

tex_node = nodes.new(type="ShaderNodeTexImage")
image_path = os.path.join(os.getcwd(), tex_filename)
tex_node.image = bpy.data.images.load(image_path)
tex_node.interpolation = 'Closest'

links.new(tex_node.outputs['Color'], bsdf.inputs['Base Color'])

if not merged_obj.data.materials:
    merged_obj.data.materials.append(mat)
else:
    merged_obj.data.materials[0] = mat

# ワークスペースファイルの保存（手動編集用）
blend_path = os.path.join(os.getcwd(), "Player_Workspace.blend")
bpy.ops.wm.save_as_mainfile(filepath=blend_path)
print(f"Saved workspace to {blend_path}")

# glTF エクスポート
export_path = os.path.join(os.getcwd(), "Player.gltf")
bpy.ops.export_scene.gltf(
    filepath=export_path,
    export_format='GLTF_SEPARATE',
    use_selection=True,
    export_image_format='AUTO'
)

print("Successfully generated detailed Player.gltf, Player_Diffuse.png, and Player_Workspace.blend")
