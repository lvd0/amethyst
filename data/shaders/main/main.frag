#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout (early_fragment_tests) in;

layout (location = 0) in VertexIn {
    vec3 i_normal;
    vec3 i_frag_pos;
    vec3 i_view_pos;
    vec2 i_uvs;
    mat3 i_TBN;
};

layout (location = 8) in flat SObjectData i_constants;

layout (location = 0) out vec4 o_pixel;

layout (std430, set = 0, binding = 4)
buffer readonly BPointLights {
    SPointLight[] point_lights;
};

layout (std430, set = 0, binding = 5)
buffer readonly BDirectionalLights {
    SDirectionalLight[] directional_lights;
};

layout (set = 0, binding = 6) uniform sampler2D[] u_textures;

vec3 calculate_normals();
vec3 calculate_specular();
vec3 calculate_point_light(in SPointLight light, in vec3 albedo, in vec3 normal, in vec3 view_dir);
vec3 calculate_directional_light(in SDirectionalLight light, in vec3 albedo, in vec3 normal, in vec3 view_dir);

void main() {
    const vec4 albedo = texture(u_textures[i_constants.albedo_index], i_uvs);
    const vec3 normal = calculate_normals();
    const vec3 view_dir = normalize(i_view_pos - i_frag_pos);
    vec3 color = albedo.rgb * AMBIENT_FACTOR;

    for (uint i = 0; i < directional_lights.length(); ++i) {
        color += calculate_directional_light(directional_lights[i], albedo.rgb, normal, view_dir);
    }
    for (uint i = 0; i < point_lights.length(); ++i) {
        color += calculate_point_light(point_lights[i], albedo.rgb, normal, view_dir);
    }
    o_pixel = vec4(color, albedo.a);
}

vec3 calculate_normals() {
    if (i_constants.normal_index == 0) {
        return normalize(i_normal);
    }
    return normalize(i_TBN * (2.0 * vec3(vec2(texture(u_textures[i_constants.normal_index], i_uvs)), 1.0) - 1.0));
}

vec3 calculate_specular() {
    if (i_constants.specular_index == 0) {
        return vec3(0.0);
    }
    return vec3(texture(u_textures[i_constants.specular_index], i_uvs));
}

vec3 calculate_point_light(in SPointLight light, in vec3 albedo, in vec3 normal, in vec3 view_dir) {
    const vec3 light_dir = normalize(light.position - i_frag_pos);
    // Diffuse.
    const float diffuse_factor = max(dot(light_dir, normal), 0.0);
    const vec3 diffuse_color = light.diffuse * diffuse_factor * albedo;
    // Specular.
    const vec3 halfway_dir = normalize(light_dir + view_dir);
    const float specular_factor = pow(max(dot(halfway_dir, normal), 0.0), 32);
    const vec3 specular_color = light.specular * specular_factor * calculate_specular();
    // Attenuation.
    const float distance = length(light.position - i_frag_pos);
    const float clamped = clamp(1.0 - pow(distance / light.radius, 4), 0, 1);
    const float attenuation = (clamped * clamped) / (1 + distance * distance);
    return (diffuse_color + specular_color) * attenuation;
}

vec3 calculate_directional_light(in SDirectionalLight light, in vec3 albedo, in vec3 normal, in vec3 view_dir) {
    const vec3 light_dir = normalize(light.direction);
    // Diffuse.
    const float diffuse_factor = max(dot(light_dir, normal), 0.0);
    const vec3 diffuse_color = light.diffuse * diffuse_factor * albedo;
    // Specular.
    const vec3 halfway_dir = normalize(light_dir + view_dir);
    const float specular_factor = pow(max(dot(halfway_dir, normal), 0.0), 32);
    const vec3 specular_color = light.specular * specular_factor * calculate_specular();

    return diffuse_color + specular_color;
}
