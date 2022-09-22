#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec4 i_tangent;

layout (set = 0, binding = 0)
uniform UCamera {
    SCameraData camera;
};

layout (set = 0, binding = 1)
buffer readonly BLocalTransforms {
    STransformData[] local_transforms;
};

layout (set = 0, binding = 2)
buffer readonly BWorldTransforms {
    STransformData[] world_transforms;
};

layout (set = 0, binding = 3, scalar)
buffer readonly BObjectData {
    SObjectData[] object_data;
};

layout (push_constant)
uniform UIndices {
    uint object_offset;
};

void main() {
    const SObjectData constants = object_data[gl_DrawID + object_offset];
    const mat4 local_transform = local_transforms[constants.transform_index[0]].transform;
    const mat4 world_transform = world_transforms[constants.transform_index[1] + gl_InstanceIndex].transform;
    const mat4 model = world_transform * local_transform;
    const vec3 frag_pos = vec3(model * vec4(i_vertex, 1.0));
    gl_Position = camera.proj_view * vec4(frag_pos, 1.0);
}
