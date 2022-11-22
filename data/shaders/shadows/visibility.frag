#version 460
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in flat uint i_instance;
layout (location = 1) in flat uint i_draw_id;

layout (location = 0) out uvec2 o_visibility;

void main() {
    o_visibility = uvec2((i_draw_id << 16u) | i_instance, gl_PrimitiveID);
}
