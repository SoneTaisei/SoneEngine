def fix():
    with open(r'Engine\GameObject\PrimitiveObject.cpp', 'rb') as f:
        content = f.read()
    text = content.decode('utf-8', errors='replace')
    try:
        raw_bytes = text.encode('cp932', errors='replace')
        text2 = raw_bytes.decode('utf-8', errors='replace')
        
        # print some lines that we know had comments
        lines = text2.split('\n')
        for i, line in enumerate(lines):
            if i in range(55, 85) and '//' in line:
                print(f"{i+1}: {line.strip()}")
                
    except Exception as e:
        print("Error cp932:", e)

if __name__ == '__main__':
    fix()
