#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec4 i_tangent;

layout (location = 0) out flat uint o_instance;
layout (location = 1) out flat uint o_draw_id;

layout (set = 0, binding = 0)
uniform UCamera {
    SCameraData current_camera;
    SCameraData previous_camera;
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

layout (set = 0, binding = 4, scalar)
buffer readonly BObjectOffsets {
    uint[] object_offsets;
};

layout (set = 0, binding = 5, scalar)
buffer readonly BObjectIDRemap {
    uint[] object_id_remap;
};

layout (set = 0, binding = 6, scalar)
buffer readonly BInstanceOffsets {
    uint[] instance_offsets;
};

layout (set = 0, binding = 7, scalar)
buffer readonly BInstanceIDRemap {
    uint[] instance_id_remap;
};

layout (push_constant)
uniform UIndices {
    uint object_offset;
};

mat4 compute_current_transform(SObjectData object, uint instance_id) {
    const mat4 local_transform = local_transforms[object.transform_index[0]].current;
    const mat4 world_transform = world_transforms[object.transform_index[1] + instance_id].current;
    return world_transform * local_transform;
}

mat4 compute_previous_transform(SObjectData object, uint instance_id) {
    const mat4 local_transform = local_transforms[object.transform_index[0]].previous;
    const mat4 world_transform = world_transforms[object.transform_index[1] + instance_id].previous;
    return world_transform * local_transform;
}

void main() {
    const uint object_id = object_id_remap[gl_DrawID + object_offsets[object_offset]];
    const uint instance_id = instance_id_remap[gl_InstanceIndex + instance_offsets[object_id]];
    const SObjectData constants = object_data[object_id];
    const mat4 current_model = compute_current_transform(constants, instance_id);
    o_instance = instance_id + 1;
    o_draw_id = object_id;
    gl_Position = current_camera.proj_view * current_model * vec4(i_vertex, 1.0);
}
