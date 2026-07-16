import sys

files = [
    r'Engine\Core\Utility\UtilityFunctions.cpp',
    r'Engine\Effect\ParticleManager.cpp',
    r'Engine\GameObject\Object3D.cpp',
    r'Engine\GameObject\PrimitiveObject.cpp',
    r'Engine\Resource\Sprite\Sprite.cpp'
]

fixes = {
    '// Resource縺ｮ險ｭ螳・': '// Resourceの設定',
    '// 笘・繝槭ロ繝ｼ繧ｸ繝｣縺九ｉ譛€譁ｰ縺ｮ繧ｫ繝｡繝ｩ諠・ｱ繧偵ご繝・ヨ・・': '// ※マネージャから最新のカメラ情報をゲット！！',
    '// (Z蝗櫁ｻ｢縺ｪ縺ｩ縺ｧ陦ｨ遉ｺ繧貞だ縺代ｉ繧後ｋ繧医≧縺ｫ縺吶ｋ縺溘ａ)': '// (Z回転などで表示を傾けられるようにするため)'
}

for f_path in files:
    with open(f_path, 'r', encoding='utf-8') as f:
        content = f.read()
        
    for k, v in fixes.items():
        if k in content:
            content = content.replace(k, v)

    with open(f_path, 'w', encoding='utf-8') as f:
        f.write(content)
