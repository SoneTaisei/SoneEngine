import os
import re

# Model.h
h_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Resource/Model/Model.h'
with open(h_path, 'r', encoding='utf-8') as f:
    h_content = f.read()
h_content = h_content.replace('void Draw();', 'void Draw(const D3D12_VERTEX_BUFFER_VIEW* weightBufferView = nullptr);')
with open(h_path, 'w', encoding='utf-8') as f:
    f.write(h_content)

# Model.cpp
cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Resource/Model/Model.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    cpp_content = f.read()

draw_old = '''void Model::Draw() {
    auto commandList = DirectXCommon::GetInstance()->GetCommandList();

    // 行列のセット（SetGraphicsRootConstantBufferView）は、
    // Object3D側で呼ぶようにするか、引数でアドレスを受け取る形にします。

    // テクスチャと頂点データのセット
    commandList->SetGraphicsRootDescriptorTable(2, textureHandle_);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    // 描画実行
    commandList->DrawIndexedInstanced(UINT(modelData_.indices.size()), 1, 0, 0, 0);
}'''

draw_new = '''void Model::Draw(const D3D12_VERTEX_BUFFER_VIEW* weightBufferView) {
    auto commandList = DirectXCommon::GetInstance()->GetCommandList();

    commandList->SetGraphicsRootDescriptorTable(2, textureHandle_);
    
    if (weightBufferView != nullptr) {
        D3D12_VERTEX_BUFFER_VIEW views[] = { vertexBufferView_, *weightBufferView };
        commandList->IASetVertexBuffers(0, 2, views);
    } else {
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    }
    
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->DrawIndexedInstanced(UINT(modelData_.indices.size()), 1, 0, 0, 0);
}'''

# Replace (handle potential minor differences in comments)
cpp_content = re.sub(r'void Model::Draw\(\) \{[\s\S]*?DrawIndexedInstanced[^\}]*\}', draw_new, cpp_content)

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(cpp_content)

print("Updated Model::Draw for skinning support.")
