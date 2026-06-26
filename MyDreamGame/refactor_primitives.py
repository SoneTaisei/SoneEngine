import os
import re

project_dir = "project"

primitives = [
    "Plane",
    "Box",
    "Sphere",
    "Circle",
    "Ring",
    "Cylinder",
    "Cone",
    "Torus",
    "Triangle",
    "Star"
]

primitive_cpp_path = os.path.join(project_dir, "Engine/Resource/Primitive/Primitive.cpp")
primitive_h_path = os.path.join(project_dir, "Engine/Resource/Primitive/Primitive.h")
primitive_manager_cpp_path = os.path.join(project_dir, "Engine/Resource/Primitive/PrimitiveManager.cpp")
vcxproj_path = os.path.join(project_dir, "MyDreamGame.vcxproj")
filters_path = os.path.join(project_dir, "MyDreamGame.vcxproj.filters")
dest_dir = os.path.join(project_dir, "Engine/Resource/Primitive")

with open(primitive_cpp_path, "r", encoding="utf-8") as f:
    cpp_content = f.read()

# Extract method bodies
method_bodies = {}
for p in primitives:
    # Pattern: void Primitive::CreateX(args...) { body }
    # This might be tricky because of nested braces. 
    # Let's use a simple brace counting parser.
    pattern = rf"void Primitive::Create{p}\((.*?)\)\s*\{{"
    match = re.search(pattern, cpp_content)
    if match:
        start_idx = match.end()
        args = match.group(1)
        brace_count = 1
        i = start_idx
        while i < len(cpp_content) and brace_count > 0:
            if cpp_content[i] == '{':
                brace_count += 1
            elif cpp_content[i] == '}':
                brace_count -= 1
            i += 1
        body = cpp_content[start_idx:i-1]
        method_bodies[p] = (args, body)

# Now, write the base Primitive class
primitive_h_new = """#pragma once
#include <d3d12.h>
#include <vector>
#include <string>
#include <wrl.h>
#include <numbers>
#include "Core/Utility/Structs.h"

enum class PrimitiveType {
    Plane,
    Box,
    Sphere,
    Circle,
    Ring,
    Cylinder,
    Cone,
    Torus,
    Triangle,
    Star
};

class Primitive {
public:
    virtual ~Primitive() = default;

    virtual void GenerateModelData() = 0;
    virtual void Initialize(ID3D12Device* device);
    void Draw(ID3D12GraphicsCommandList* commandList);

    const ModelData& GetModelData() const { return modelData_; }

protected:
    void CreateBuffers(ID3D12Device* device);

    ModelData modelData_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
};
"""

primitive_cpp_new = """#include "Primitive.h"
#include <cassert>

void Primitive::Initialize(ID3D12Device* device) {
    modelData_.vertices.clear();
    modelData_.indices.clear();

    GenerateModelData();
    CreateBuffers(device);
}

void Primitive::Draw(ID3D12GraphicsCommandList* commandList) {
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->DrawIndexedInstanced(static_cast<UINT>(modelData_.indices.size()), 1, 0, 0, 0);
}

void Primitive::CreateBuffers(ID3D12Device* device) {
    HRESULT hr;
    vertexResource_ = CreateBufferResource(device, sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    assert(SUCCEEDED(hr));
    std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
    vertexResource_->Unmap(0, nullptr);

    indexResource_ = CreateBufferResource(device, sizeof(uint32_t) * modelData_.indices.size());
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * modelData_.indices.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* indexData = nullptr;
    hr = indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
    assert(SUCCEEDED(hr));
    std::memcpy(indexData, modelData_.indices.data(), sizeof(uint32_t) * modelData_.indices.size());
    indexResource_->Unmap(0, nullptr);
}
"""

with open(primitive_h_path, "w", encoding="utf-8") as f:
    f.write(primitive_h_new)
with open(primitive_cpp_path, "w", encoding="utf-8") as f:
    f.write(primitive_cpp_new)

# Generate individual files
new_cpps = []
new_hs = []
for p in primitives:
    args, body = method_bodies[p]
    
    # Parse args to member variables
    arg_list = [a.strip() for a in args.split(",") if a.strip()]
    members = []
    ctor_args = []
    init_list = []
    for arg in arg_list:
        parts = arg.split("=")
        decl = parts[0].strip()
        def_val = parts[1].strip() if len(parts) > 1 else ""
        words = decl.split()
        t = " ".join(words[:-1])
        name = words[-1]
        members.append(f"{t} {name}_;")
        if def_val:
            ctor_args.append(f"{t} {name} = {def_val}")
        else:
            ctor_args.append(f"{t} {name}")
        init_list.append(f"{name}_({name})")

    ctor_str = ", ".join(ctor_args)
    init_str = " : " + ", ".join(init_list) if init_list else ""
    members_str = "\n    ".join(members)

    h_content = f"""#pragma once
#include "Primitive.h"

class Primitive{p} : public Primitive {{
public:
    Primitive{p}({ctor_str});
    void GenerateModelData() override;

private:
    {members_str}
}};
"""
    cpp_content = f"""#include "Primitive{p}.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <numbers>

Primitive{p}::Primitive{p}({", ".join([a.split("=")[0].strip() for a in ctor_args])}){init_str} {{
}}

void Primitive{p}::GenerateModelData() {{
"""
    # Replace variable usages in body with member variables
    for arg in arg_list:
        name = arg.split("=")[0].strip().split()[-1]
        # Regex to replace standalone variables
        body = re.sub(rf'\b{name}\b', f"{name}_", body)
    
    cpp_content += body + "\n}\n"

    with open(os.path.join(dest_dir, f"Primitive{p}.h"), "w", encoding="utf-8") as f:
        f.write(h_content)
    with open(os.path.join(dest_dir, f"Primitive{p}.cpp"), "w", encoding="utf-8") as f:
        f.write(cpp_content)

    new_cpps.append(f"Engine\\Resource\\Primitive\\Primitive{p}.cpp")
    new_hs.append(f"Engine\\Resource\\Primitive\\Primitive{p}.h")

# Update PrimitiveManager.cpp
manager_h_includes = "\n".join([f'#include "Primitive{p}.h"' for p in primitives])

manager_cpp_new = f"""#include "PrimitiveManager.h"
{manager_h_includes}
#include <format>

void PrimitiveManager::Initialize(ID3D12Device* device) {{
    device_ = device;
}}

Primitive* PrimitiveManager::GetPrimitive(PrimitiveType type, float size, uint32_t segments) {{
    std::string key = std::format("{{}}_{{:.2f}}_{{}}", static_cast<int>(type), size, segments);

    if (primitiveRegistry_.contains(key)) {{
        return primitiveRegistry_[key].get();
    }}

    std::unique_ptr<Primitive> primitive;
    switch (type) {{
        case PrimitiveType::Plane: primitive = std::make_unique<PrimitivePlane>(size); break;
        case PrimitiveType::Box: primitive = std::make_unique<PrimitiveBox>(size); break;
        case PrimitiveType::Sphere: primitive = std::make_unique<PrimitiveSphere>(size, segments); break;
        case PrimitiveType::Circle: primitive = std::make_unique<PrimitiveCircle>(size, segments); break;
        case PrimitiveType::Ring: primitive = std::make_unique<PrimitiveRing>(size * 0.5f, size, segments); break;
        case PrimitiveType::Cylinder: primitive = std::make_unique<PrimitiveCylinder>(size, size * 2.0f, segments); break;
        case PrimitiveType::Cone: primitive = std::make_unique<PrimitiveCone>(size, size * 2.0f, segments); break;
        case PrimitiveType::Torus: primitive = std::make_unique<PrimitiveTorus>(size, size * 0.3f, segments); break;
        case PrimitiveType::Triangle: primitive = std::make_unique<PrimitiveTriangle>(size); break;
        case PrimitiveType::Star: primitive = std::make_unique<PrimitiveStar>(size); break;
    }}

    if (primitive) {{
        primitive->Initialize(device_);
        primitiveRegistry_[key] = std::move(primitive);
        return primitiveRegistry_[key].get();
    }}
    return nullptr;
}}

Primitive* PrimitiveManager::GetRing(float innerRadius, float outerRadius, uint32_t segments, float startAngle, float endAngle, const Vector4& innerColor, const Vector4& outerColor, bool isRadialUV) {{
    std::string key = std::format("Ring_R{{:.2f}}_r{{:.2f}}_S{{}}_A{{:.2f}}_a{{:.2f}}_C{{:.2f}}{{:.2f}}{{:.2f}}{{:.2f}}_c{{:.2f}}{{:.2f}}{{:.2f}}{{:.2f}}_{{}}",
        outerRadius, innerRadius, segments, startAngle, endAngle,
        innerColor.x, innerColor.y, innerColor.z, innerColor.w,
        outerColor.x, outerColor.y, outerColor.z, outerColor.w,
        isRadialUV ? 1 : 0);

    if (primitiveRegistry_.contains(key)) {{
        return primitiveRegistry_[key].get();
    }}

    auto primitive = std::make_unique<PrimitiveRing>(innerRadius, outerRadius, segments, startAngle, endAngle, innerColor, outerColor, isRadialUV);
    primitive->Initialize(device_);
    primitiveRegistry_[key] = std::move(primitive);

    return primitiveRegistry_[key].get();
}}
"""

with open(primitive_manager_cpp_path, "w", encoding="utf-8") as f:
    f.write(manager_cpp_new)

# Update vcxproj
with open(vcxproj_path, "r", encoding="utf-8") as f:
    vcxproj = f.read()

# Insert before </ItemGroup> containing ClCompile
# Find the end of ClCompile ItemGroup
clcompile_idx = vcxproj.rfind("</ClCompile>")
if clcompile_idx != -1:
    group_end = vcxproj.find("</ItemGroup>", clcompile_idx)
    insert_str = "\n".join([f'    <ClCompile Include="{c}" />' for c in new_cpps]) + "\n"
    vcxproj = vcxproj[:group_end] + insert_str + vcxproj[group_end:]

# Find the end of ClInclude ItemGroup
clinclude_idx = vcxproj.rfind("</ClInclude>")
if clinclude_idx != -1:
    group_end = vcxproj.find("</ItemGroup>", clinclude_idx)
    insert_str = "\n".join([f'    <ClInclude Include="{h}" />' for h in new_hs]) + "\n"
    vcxproj = vcxproj[:group_end] + insert_str + vcxproj[group_end:]

with open(vcxproj_path, "w", encoding="utf-8") as f:
    f.write(vcxproj)

# Update filters
with open(filters_path, "r", encoding="utf-8") as f:
    filters = f.read()

clcompile_idx = filters.rfind("</ClCompile>")
if clcompile_idx != -1:
    group_end = filters.find("</ItemGroup>", clcompile_idx)
    insert_str = ""
    for c in new_cpps:
        insert_str += f"""    <ClCompile Include="{c}">
      <Filter>Engine\\Graphics\\Object</Filter>
    </ClCompile>\n"""
    filters = filters[:group_end] + insert_str + filters[group_end:]

clinclude_idx = filters.rfind("</ClInclude>")
if clinclude_idx != -1:
    group_end = filters.find("</ItemGroup>", clinclude_idx)
    insert_str = ""
    for h in new_hs:
        insert_str += f"""    <ClInclude Include="{h}">
      <Filter>Engine\\Graphics\\Object</Filter>
    </ClInclude>\n"""
    filters = filters[:group_end] + insert_str + filters[group_end:]

with open(filters_path, "w", encoding="utf-8") as f:
    f.write(filters)

print("Refactoring complete.")
