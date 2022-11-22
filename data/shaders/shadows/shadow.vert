#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shader_viewport_layer_array : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec4 i_tangent;

layout (set = 0, binding = 0)
uniform UShadowCascades {
    SShadowCascade[AM_GLSL_MAX_CASCADES] shadow_cascades;
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
    uint shadow_layer;
};

mat4 compute_current_transform(SObjectData object, uint instance_id) {
    const mat4 local_transform = local_transforms[object.transform_index[0]].current;
    const mat4 world_transform = world_transforms[object.transform_index[1] + instance_id].current;
    return world_transform * local_transform;
}

void main() {
    const SObjectData constants = object_data[gl_DrawID + object_offset];
    const mat4 current_model = compute_current_transform(constants, gl_InstanceIndex);
    gl_Position = shadow_cascades[shadow_layer].proj_view * current_model * vec4(i_vertex, 1.0);
    gl_Layer = int(shadow_layer);
}
