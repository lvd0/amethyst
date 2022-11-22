#version 460
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_control_flow_attributes : enable
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

layout (location = 0) out vec4 o_pixel;

layout (location = 0) in vec2 i_uvs;

layout (buffer_reference, scalar)
buffer readonly Vertices {
    SVertexData[] vertices;
};

layout (buffer_reference, scalar)
buffer readonly Indices {
    uint[] indices;
};

layout (set = 0, binding = 0, scalar)
buffer readonly BObjectData {
    SObjectData[] objects;
};

layout (set = 0, binding = 1)
uniform UCamera {
    SCameraData camera;
    SCameraData prev_camera;
};

layout (set = 0, binding = 2)
buffer readonly BLocalTransforms {
    STransformData[] local_transforms;
};

layout (set = 0, binding = 3)
buffer readonly BWorldTransforms {
    STransformData[] world_transforms;
};

layout (set = 0, binding = 4, rg32ui) uniform uimage2D u_visibility;

layout (set = 0, binding = 5) uniform sampler2D[] u_textures;

layout (std430, set = 1, binding = 0)
buffer readonly BPointLights {
    SPointLight[] point_lights;
};

layout (std430, set = 1, binding = 1)
buffer readonly BDirectionalLights {
    SDirectionalLight[] directional_lights;
};

layout (push_constant)
uniform UIndices {
    uint object_count;
};

struct SPartialDerivative {
    vec3 lambda;
    vec3 ddx;
    vec3 ddy;
};

struct SUVDeriv {
    vec2 uvs;
    vec2 ddx;
    vec2 ddy;
};

SPartialDerivative compute_derivatives(in vec4[3] attributes, in vec2 pixel, in vec2 screen_size) {
    SPartialDerivative result;
    const vec3 inv_w = 1 / vec3(attributes[0].w, attributes[1].w, attributes[2].w);
    const vec2 ndc0 = attributes[0].xy * inv_w.x;
    const vec2 ndc1 = attributes[1].xy * inv_w.y;
    const vec2 ndc2 = attributes[2].xy * inv_w.z;

    const float inv_det = 1 / determinant(mat2(ndc2 - ndc1, ndc0 - ndc1));
    result.ddx = vec3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y) * inv_det * inv_w;
    result.ddy = vec3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x) * inv_det * inv_w;
    float ddx_sum = dot(result.ddx, vec3(1));
    float ddy_sum = dot(result.ddy, vec3(1));

    const vec2 delta_v = pixel - ndc0;
    const float interp_inv_w = inv_w.x + delta_v.x * ddx_sum + delta_v.y * ddy_sum;
    const float interp_w = 1 / interp_inv_w;

    result.lambda = vec3(
        interp_w * (inv_w.x + delta_v.x * result.ddx.x + delta_v.y * result.ddy.x),
        interp_w * (delta_v.x * result.ddx.y + delta_v.y * result.ddy.y),
        interp_w * (delta_v.x * result.ddx.z + delta_v.y * result.ddy.z));

    result.ddx *= 2.0 / screen_size.x;
    result.ddy *= 2.0 / screen_size.y;
    ddx_sum *= 2.0 / screen_size.x;
    ddy_sum *= 2.0 / screen_size.y;

    result.ddy *= -1.0;
    ddy_sum *= -1.0;

    const float interp_w_ddx = 1.0 / (interp_inv_w + ddx_sum);
    const float interp_w_ddy = 1.0 / (interp_inv_w + ddy_sum);

    result.ddx = interp_w_ddx * (result.lambda * interp_inv_w + result.ddx) - result.lambda;
    result.ddy = interp_w_ddy * (result.lambda * interp_inv_w + result.ddy) - result.lambda;

    return result;
}

vec3 interpolate_attributes(in SPartialDerivative derivatives, in float[3] attributes) {
    const vec3 merged_v = vec3(attributes[0], attributes[1], attributes[2]);
    return vec3(
        dot(merged_v, derivatives.lambda),
        dot(merged_v, derivatives.ddx),
        dot(merged_v, derivatives.ddy));
}

vec3 interpolate_attributes(in SPartialDerivative derivatives, in vec3[3] attributes) {
    return
        attributes[0] * derivatives.lambda.x +
        attributes[1] * derivatives.lambda.y +
        attributes[2] * derivatives.lambda.z;
}

vec4 interpolate_attributes(in SPartialDerivative derivatives, in vec4[3] attributes) {
    return
        attributes[0] * derivatives.lambda.x +
        attributes[1] * derivatives.lambda.y +
        attributes[2] * derivatives.lambda.z;
}

vec3 compute_normals(in uint object_id, in mat3 TBN, in vec3 a_normal, in SUVDeriv a_uvs) {
    const uint normal_index = objects[object_id].normal_index;
    if (normal_index == 0) {
        return normalize(a_normal);
    }
    const vec3 normal = vec3(textureGrad(u_textures[normal_index], a_uvs.uvs, a_uvs.ddx, a_uvs.ddy).rg, 1.0);
    return normalize(TBN * (2.0 * normal - 1.0));
}

vec3 compute_specular(in uint object_id, in SUVDeriv a_uvs) {
    const uint specular_index = objects[object_id].specular_index;
    if (specular_index == 0) {
        return vec3(0.0);
    }
    return vec3(textureGrad(u_textures[specular_index], a_uvs.uvs, a_uvs.ddx, a_uvs.ddy));
}

vec3 calculate_point_light(in SPointLight light, in vec3 albedo, in vec3 normal, in vec3 specular, in vec3 view_dir, in vec3 frag_pos) {
    const vec3 light_dir = normalize(light.position - frag_pos);
    // Diffuse.
    const float diffuse_factor = max(dot(light_dir, normal), 0.0);
    const vec3 diffuse_color = light.diffuse * diffuse_factor * albedo;
    // Specular.
    const vec3 halfway_dir = normalize(light_dir + view_dir);
    const float specular_factor = pow(max(dot(halfway_dir, normal), 0.0), 32);
    const vec3 specular_color = light.specular * specular_factor * specular;
    // Attenuation.
    const float distance = length(light.position - frag_pos);
    const float clamped = clamp(1.0 - pow(distance / light.radius, 4), 0, 1);
    const float attenuation = (clamped * clamped) / (1 + distance * distance);
    return (diffuse_color + specular_color) * attenuation;
}

vec3 calculate_directional_light(in SDirectionalLight light, in vec3 albedo, in vec3 normal, in vec3 specular, in vec3 view_dir) {
    const vec3 light_dir = normalize(light.direction);
    // Diffuse.
    const float diffuse_factor = max(dot(light_dir, normal), 0.0);
    const vec3 diffuse_color = light.diffuse * diffuse_factor * albedo;
    // Specular.
    const vec3 halfway_dir = normalize(light_dir + view_dir);
    const float specular_factor = pow(max(dot(halfway_dir, normal), 0.0), 32);
    const vec3 specular_color = light.specular * specular_factor * specular;

    return diffuse_color + specular_color;
}

vec3 as_srgb(in vec3 color) {
    vec3 result = color;
    [[unroll]]
    for (uint i = 0; i < 3; ++i) {
        if (result[i] <= 0.0031308) {
            result[i] *= 12.92;
        } else {
            result[i] = 1.055 * pow(result[i], 0.41666) - 0.055;
        }
    }
    return result;
}

void main() {
    if (object_count == 0) {
        discard;
    }
    const ivec2 visibility_size = imageSize(u_visibility);
    const ivec2 visibility_coords = ivec2(
        i_uvs.x * visibility_size.x - 0.5,
        (1 - i_uvs.y) * visibility_size.y - 0.5);
    const uvec2 visibility_data = imageLoad(u_visibility, visibility_coords).rg;
    if (visibility_data == uvec2(0)) {
        discard;
    }
    const vec2 ndc_uvs = i_uvs * 2.0 - 1.0;
    const uint object_id = (visibility_data.x >> 16u) & 0xffffu;
    const uint instance_id = (visibility_data.x & 0xffffu) - 1;
    const uint primitive_id = visibility_data.y;

    const SObjectData object = objects[object_id];
    const int vertex_offset = object.vertex_offset;
    const uint index_offset = object.index_offset;
    const mat4 local_transform = local_transforms[object.transform_index[0]].current;
    const mat4 world_transform = world_transforms[object.transform_index[1] + instance_id].current;
    const mat4 model = world_transform * local_transform;
    Vertices vertex_ptr = Vertices(object.vertex_address);
    Indices index_ptr = Indices(object.index_address);
    const uint[] indices = uint[](
        index_ptr.indices[3 * (index_offset + primitive_id) + 0],
        index_ptr.indices[3 * (index_offset + primitive_id) + 1],
        index_ptr.indices[3 * (index_offset + primitive_id) + 2]);
    const SVertexData[] vertices = SVertexData[](
        vertex_ptr.vertices[vertex_offset + indices[0]],
        vertex_ptr.vertices[vertex_offset + indices[1]],
        vertex_ptr.vertices[vertex_offset + indices[2]]);

    const vec3 raw_vertex_0 = vertices[0].position;
    const vec3 raw_vertex_1 = vertices[1].position;
    const vec3 raw_vertex_2 = vertices[2].position;
    const vec4 clip_vertex_0 = camera.proj_view * model * vec4(raw_vertex_0, 1.0);
    const vec4 clip_vertex_1 = camera.proj_view * model * vec4(raw_vertex_1, 1.0);
    const vec4 clip_vertex_2 = camera.proj_view * model * vec4(raw_vertex_2, 1.0);
    const SPartialDerivative derivatives = compute_derivatives(vec4[](clip_vertex_0, clip_vertex_1, clip_vertex_2), ndc_uvs, visibility_size);

    const vec3 a_position = interpolate_attributes(derivatives, vec3[](raw_vertex_0, raw_vertex_1, raw_vertex_2));

    const vec3 raw_normal_0 = vertices[0].normal;
    const vec3 raw_normal_1 = vertices[1].normal;
    const vec3 raw_normal_2 = vertices[2].normal;
    const vec3 a_normal = normalize(interpolate_attributes(derivatives, vec3[](raw_normal_0, raw_normal_1, raw_normal_2)));

    const vec2 raw_uv_0 = vertices[0].uv;
    const vec2 raw_uv_1 = vertices[1].uv;
    const vec2 raw_uv_2 = vertices[2].uv;
    const vec3[] interp_uv = vec3[](
        interpolate_attributes(derivatives, float[](raw_uv_0.x, raw_uv_1.x, raw_uv_2.x)),
        interpolate_attributes(derivatives, float[](raw_uv_0.y, raw_uv_1.y, raw_uv_2.y)));
    SUVDeriv a_uvs;
    a_uvs.uvs = vec2(interp_uv[0].x, interp_uv[1].x);
    a_uvs.ddx = vec2(interp_uv[0].y, interp_uv[1].y);
    a_uvs.ddy = vec2(interp_uv[0].z, interp_uv[1].z);

    const vec4 raw_tangent_0 = vertices[0].tangent;
    const vec4 raw_tangent_1 = vertices[1].tangent;
    const vec4 raw_tangent_2 = vertices[2].tangent;
    const vec4 a_tangent = normalize(interpolate_attributes(derivatives, vec4[](raw_tangent_0, raw_tangent_1, raw_tangent_2)));
    mat3 TBN;
    mat3 inv_model = mat3(transpose(inverse(model)));
    {
        const vec3 T = normalize(inv_model * vec3(a_tangent));
        const vec3 B = normalize(inv_model * cross(a_normal, vec3(a_tangent)) * a_tangent.w);
        const vec3 N = normalize(inv_model * a_normal);
        TBN = mat3(T, B, N);
    }

    const vec3 albedo = textureGrad(u_textures[object.albedo_index], a_uvs.uvs, a_uvs.ddx, a_uvs.ddy).rgb;
    const vec3 normal = compute_normals(object_id, TBN, inv_model * a_normal, a_uvs);
    const vec3 specular = compute_specular(object_id, a_uvs);
    const vec3 frag_pos = vec3(model * vec4(a_position, 1.0));
    const vec3 view_dir = normalize(vec3(camera.position) - frag_pos);
    vec3 color = albedo * AMBIENT_FACTOR;

    for (uint i = 0; i < directional_lights.length(); ++i) {
        color += calculate_directional_light(directional_lights[i], albedo, normal, specular, view_dir);
    }
    for (uint i = 0; i < point_lights.length(); ++i) {
        color += calculate_point_light(point_lights[i], albedo, normal, specular, view_dir, frag_pos);
    }

    o_pixel = vec4(as_srgb(color), 1.0);
}
