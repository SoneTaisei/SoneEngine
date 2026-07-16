import os, glob

def check():
    files = glob.glob('**/*.cpp', recursive=True) + \
            glob.glob('**/*.h', recursive=True) + \
            glob.glob('**/*.hlsl', recursive=True) + \
            glob.glob('**/*.hlsli', recursive=True)
    
    res = {'utf-8': 0, 'shift-jis': 0, 'ascii': 0, 'unknown': 0}
    sjis_files = []
    hlsl_sjis = []
    
    for f in files:
        with open(f, 'rb') as file:
            content = file.read()
        
        try:
            content.decode('ascii')
            res['ascii'] += 1
            continue
        except UnicodeDecodeError:
            pass
            
        try:
            content.decode('utf-8')
            res['utf-8'] += 1
            continue
        except UnicodeDecodeError:
            pass
            
        try:
            content.decode('shift_jis')
            res['shift-jis'] += 1
            sjis_files.append(f)
            if f.endswith('.hlsl') or f.endswith('.hlsli'):
                hlsl_sjis.append(f)
            continue
        except UnicodeDecodeError:
            res['unknown'] += 1
            print(f'Unknown encoding: {f}')
            
    print(res)
    print('Shift-JIS files:', len(sjis_files))
    print('HLSL Shift-JIS files:', hlsl_sjis)
    
    # Save the list of files to convert
    with open('sjis_files.txt', 'w', encoding='utf-8') as f:
        for p in sjis_files:
            f.write(p + '\n')

if __name__ == '__main__':
    check()
