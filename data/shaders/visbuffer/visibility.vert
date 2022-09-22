#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec4 i_tangent;

layout (location = 0) out flat uint o_instance;
layout (location = 1) out flat uint o_draw_id;
layout (location = 2) out flat vec3 o_current_position;
layout (location = 3) out flat vec3 o_previous_position;

layout (set = 0, binding = 0)
uniform UCamera {
    SCameraData current_camera;
    SCameraData previous_camera;
};

layout (set = 0, binding = 1, scalar)
uniform UJitter {
    vec2[16] jitter;
};

layout (set = 0, binding = 2)
buffer readonly BLocalTransforms {
    STransformData[] local_transforms;
};

layout (set = 0, binding = 3)
buffer readonly BWorldTransforms {
    STransformData[] world_transforms;
};

layout (set = 0, binding = 4)
buffer readonly BObjectData {
    SObjectData[] object_data;
};

layout (push_constant)
uniform UIndices {
    uint object_offset;
    vec2 jitter_offset;
};

mat4 compute_current_transform(SObjectData constants) {
    const mat4 local_transform = local_transforms[constants.transform_index[0]].current;
    const mat4 world_transform = world_transforms[constants.transform_index[1] + gl_InstanceIndex].current;
    return world_transform * local_transform;
}

mat4 compute_previous_transform(SObjectData constants) {
    const mat4 local_transform = local_transforms[constants.transform_index[0]].previous;
    const mat4 world_transform = world_transforms[constants.transform_index[1] + gl_InstanceIndex].previous;
    return world_transform * local_transform;
}

void main() {
    const SObjectData constants = object_data[gl_DrawID + object_offset];
    const mat4 current_model = compute_current_transform(constants);
    const mat4 previous_model = compute_previous_transform(constants);
    const vec4 clip_pos = current_camera.proj_view * current_model * vec4(i_vertex, 1.0);
    const vec4 prev_clip_pos = previous_camera.proj_view * previous_model * vec4(i_vertex, 1.0);
    o_current_position = clip_pos.xyz / clip_pos.w;
    o_previous_position = prev_clip_pos.xyz / prev_clip_pos.w;
    o_instance = gl_InstanceIndex + 1;
    o_draw_id = gl_DrawID + object_offset;
    gl_Position = clip_pos;
    gl_Position.xy += jitter_offset;
}
