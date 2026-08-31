import bpy
import mathutils
import gpu
import gpu_extras.batch
import copy

# --- 描画処理: コライダー ---
class DrawCollider:
    handle = None

    @classmethod
    def draw_collider(cls):
        vertices = {"pos": []}
        indices = []
        
        # offsets for the 8 vertices of a box
        offsets = [
            [-0.5, -0.5, -0.5], # 0
            [ 0.5, -0.5, -0.5], # 1
            [ 0.5,  0.5, -0.5], # 2
            [-0.5,  0.5, -0.5], # 3
            [-0.5, -0.5,  0.5], # 4
            [ 0.5, -0.5,  0.5], # 5
            [ 0.5,  0.5,  0.5], # 6
            [-0.5,  0.5,  0.5], # 7
        ]
        
        # iterate over all objects in the current scene
        for object in bpy.context.scene.objects:
            # skip drawing if the object doesn't have a collider property
            if not "collider" in object:
                continue
                
            center = mathutils.Vector((0,0,0))
            size = mathutils.Vector((2,2,2))
            
            center[0]=object["collider_center"][0]
            center[1]=object["collider_center"][1]
            center[2]=object["collider_center"][2]
            size[0]=object["collider_size"][0]
            size[1]=object["collider_size"][1]
            size[2]=object["collider_size"][2]
            
            start = len(vertices["pos"])
            
            # calculate position for each vertex
            for offset in offsets:
                pos = copy.copy(center)
                pos[0]+=offset[0]*size[0]
                pos[1]+=offset[1]*size[1]
                pos[2]+=offset[2]*size[2]
                
                # convert from local coordinates to world coordinates
                pos = object.matrix_world @ pos
                vertices['pos'].append(pos)
                
            # append indices for the 12 edges of the box
            indices.extend([
                (start+0, start+1), (start+1, start+2), (start+2, start+3), (start+3, start+0),
                (start+4, start+5), (start+5, start+6), (start+6, start+7), (start+7, start+4),
                (start+0, start+4), (start+1, start+5), (start+2, start+6), (start+3, start+7)
            ])
            
        if len(vertices["pos"]) > 0:
            if bpy.app.version >= (4, 0, 0):
                shader = gpu.shader.from_builtin('UNIFORM_COLOR')
            else:
                shader = gpu.shader.from_builtin('3D_UNIFORM_COLOR')
            batch = gpu_extras.batch.batch_for_shader(shader, 'LINES', {"pos": vertices["pos"]}, indices=indices)
            shader.bind()
            shader.uniform_float("color", (0.5, 1.0, 0.5, 1.0)) # Draw green box
            batch.draw(shader)
