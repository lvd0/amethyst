#version 460
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in flat uint i_instance;
layout (location = 1) in flat uint i_draw_id;
layout (location = 2) in flat vec3 i_current_position;
layout (location = 3) in flat vec3 i_previous_position;

layout (location = 0) out uvec2 o_visibility;
layout (location = 1) out vec2 o_velocity;

void main() {
    o_visibility = uvec2((i_draw_id << 16u) | i_instance, gl_PrimitiveID);
    o_velocity = i_current_position.xy - i_previous_position.xy;
}
