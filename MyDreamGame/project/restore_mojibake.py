import sys
import difflib

file_pairs = [
    ('old_Utility.cpp', r'Engine\Core\Utility\UtilityFunctions.cpp'),
    ('old_Particle.cpp', r'Engine\Effect\ParticleManager.cpp'),
    ('old_Object.cpp', r'Engine\GameObject\Object3D.cpp'),
    ('old_Primitive.cpp', r'Engine\GameObject\PrimitiveObject.cpp'),
    ('old_Sprite.cpp', r'Engine\Resource\Sprite\Sprite.cpp')
]

for old_f, new_f in file_pairs:
    with open(old_f, 'rb') as f:
        # read utf-16le, and strip BOM from the very first character if present
        text = f.read().decode('utf-16le')
        if text.startswith('\ufeff'):
            text = text[1:]
        old_lines = text.splitlines(keepends=True)
        
    with open(new_f, 'rb') as f:
        new_text = f.read().decode('utf-8')
        if new_text.startswith('\ufeff'):
            new_text = new_text[1:]
        new_lines = new_text.splitlines(keepends=True)
    
    def has_japanese(text):
        return any(ord(c) > 127 for c in text)
        
    def has_mojibake(text):
        return '蝗' in text or '縺' in text or '・' in text or '譁' in text or '' in text
        
    result_lines = list(new_lines)
    
    for i, n_line in enumerate(result_lines):
        if has_mojibake(n_line):
            best_match = None
            best_ratio = 0
            for o_line in old_lines:
                if has_japanese(o_line):
                    n_ascii = ''.join(c for c in n_line if ord(c) <= 127)
                    o_ascii = ''.join(c for c in o_line if ord(c) <= 127)
                    ratio = difflib.SequenceMatcher(None, n_ascii, o_ascii).ratio()
                    if ratio > best_ratio:
                        best_ratio = ratio
                        best_match = o_line
            if best_match and best_ratio > 0.8:
                result_lines[i] = best_match

    # Save the repaired file
    with open(new_f, 'w', encoding='utf-8') as f:
        f.write('\ufeff')
        for line in result_lines:
            f.write(line)

print("Done")
