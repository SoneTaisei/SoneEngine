import sys

files = [
    r'Engine\Core\Utility\UtilityFunctions.cpp',
    r'Engine\Effect\ParticleManager.cpp',
    r'Engine\GameObject\Object3D.cpp',
    r'Engine\GameObject\PrimitiveObject.cpp',
    r'Engine\Resource\Sprite\Sprite.cpp'
]

with open('mojibake_guessed.txt', 'w', encoding='utf-8') as out:
    for f in files:
        with open(f, 'r', encoding='utf-8', errors='ignore') as file:
            lines = file.readlines()
        
        out.write(f"--- {f} ---\n")
        for i, line in enumerate(lines):
            # Check for double-mojibake traits
            if '蝗' in line or '縺' in line or '譁' in line or '・' in line:
                raw = line.encode('cp932', errors='ignore')
                guessed = raw.decode('utf-8', errors='ignore')
                out.write(f"{i+1}: {guessed.strip()}\n")
