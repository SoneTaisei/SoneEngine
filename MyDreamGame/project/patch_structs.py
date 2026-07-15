import os

structs_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Core/Utility/Structs.h'
with open(structs_path, 'r', encoding='utf-8') as f:
    content = f.read()

# VertexWeightData構造体をVertexDataの下に追加
if 'struct VertexWeightData {' not in content:
    vertex_data_end = 'struct VertexData {\n\tVector4 position;\n\tVector2 texcoord;\n\tVector3 normal;\n\tVector4 color;\n};\n'
    vertex_weight_data = '\nstruct VertexWeightData {\n    float weight[4];\n    int32_t jointIndices[4];\n};\n'
    content = content.replace(vertex_data_end, vertex_data_end + vertex_weight_data)

    with open(structs_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print("Added VertexWeightData to Structs.h")
else:
    print("VertexWeightData already exists.")
