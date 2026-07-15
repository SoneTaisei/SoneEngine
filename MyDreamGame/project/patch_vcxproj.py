import os
import re

vcxproj_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/MyDreamGame.vcxproj'
filters_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/MyDreamGame.vcxproj.filters'

# vcxprojの更新
with open(vcxproj_path, 'r', encoding='utf-8') as f:
    content = f.read()

if 'SkeletonDebugRenderer.cpp' not in content:
    content = content.replace('<ClCompile Include="Engine\\Renderer\\Renderer.cpp" />', '<ClCompile Include="Engine\\Renderer\\Renderer.cpp" />\n    <ClCompile Include="Engine\\Renderer\\SkeletonDebugRenderer.cpp" />')
if 'SkeletonDebugRenderer.h' not in content:
    content = content.replace('<ClInclude Include="Engine\\Renderer\\Renderer.h" />', '<ClInclude Include="Engine\\Renderer\\Renderer.h" />\n    <ClInclude Include="Engine\\Renderer\\SkeletonDebugRenderer.h" />')

with open(vcxproj_path, 'w', encoding='utf-8') as f:
    f.write(content)

# filtersの更新
with open(filters_path, 'r', encoding='utf-8') as f:
    content = f.read()

if 'SkeletonDebugRenderer.cpp' not in content:
    cpp_filter = r'''<ClCompile Include="Engine\Renderer\SkeletonDebugRenderer.cpp">
      <Filter>Engine\Renderer</Filter>
    </ClCompile>'''
    content = content.replace('<ClCompile Include="Engine\\Renderer\\Renderer.cpp">', cpp_filter + '\n    <ClCompile Include="Engine\\Renderer\\Renderer.cpp">')

if 'SkeletonDebugRenderer.h' not in content:
    h_filter = r'''<ClInclude Include="Engine\Renderer\SkeletonDebugRenderer.h">
      <Filter>Engine\Renderer</Filter>
    </ClInclude>'''
    content = content.replace('<ClInclude Include="Engine\\Renderer\\Renderer.h">', h_filter + '\n    <ClInclude Include="Engine\\Renderer\\Renderer.h">')

with open(filters_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Added SkeletonDebugRenderer to vcxproj")
