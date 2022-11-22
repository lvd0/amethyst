#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_debug_printf : enable

layout (location = 0) in vec2 i_uvs;

layout (location = 0) out vec4 o_pixel;

layout (set = 0, binding = 0) uniform sampler2D u_taa_history;
layout (set = 0, binding = 1) uniform sampler2D u_taa_velocity;
layout (set = 0, binding = 2) uniform sampler2D u_final_color;
layout (set = 0, binding = 3) uniform sampler2D u_scene_depth;

layout (push_constant)
uniform Constants {
    uint ignore_history;
};

float compute_mitchell(in float x, in float b, in float c) {
    float y = 0.0;
    const float x2 = x * x;
    const float x3 = x * x * x;
    if (x < 1) {
        y = (12 - 9 * b - 6 * c) * x3 + (-18 + 12 * b + 6 * c) * x2 + (6 - 2 * b);
    } else if (x <= 2) {
        y = (-b - 6 * c) * x3 + (6 * b + 30 * c) * x2 + (-12 * b - 48 * c) * x + (8 * b + 24 * c);
    }
    return y / 6.0;
}

vec4 sample_catmull_rom(in sampler2D image, in vec2 uvs) {
    const ivec2 resolution = textureSize(image, 0);
    const vec2 sample_position = uvs * resolution;
    const vec2 uvs_1 = floor(sample_position - 0.5) + 0.5;
    const vec2 f = sample_position - uvs_1;

    const vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    const vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    const vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    const vec2 w3 = f * f * (-0.5 + 0.5 * f);

    const vec2 w12 = w1 + w2;
    const vec2 offset12 = w2 / (w1 + w2);

    vec2 uvs_0 = uvs_1 - 1;
    vec2 uvs_3 = uvs_1 + 2;
    vec2 uvs_12 = uvs_1 + offset12;

    uvs_0 /= resolution;
    uvs_3 /= resolution;
    uvs_12 /= resolution;

    vec4 result = vec4(0.0);
    result += texture(image, vec2(uvs_0.x, uvs_0.y)) * w0.x * w0.y;
    result += texture(image, vec2(uvs_12.x, uvs_0.y)) * w12.x * w0.y;
    result += texture(image, vec2(uvs_3.x, uvs_0.y)) * w3.x * w0.y;

    result += texture(image, vec2(uvs_0.x, uvs_12.y)) * w0.x * w12.y;
    result += texture(image, vec2(uvs_12.x, uvs_12.y)) * w12.x * w12.y;
    result += texture(image, vec2(uvs_3.x, uvs_12.y)) * w3.x * w12.y;

    result += texture(image, vec2(uvs_0.x, uvs_3.y)) * w0.x * w3.y;
    result += texture(image, vec2(uvs_12.x, uvs_3.y)) * w12.x * w3.y;
    result += texture(image, vec2(uvs_3.x, uvs_3.y)) * w3.x * w3.y;
    return result;
}

vec4 compute_aabb_clip(in vec3 aabb_min, in vec3 aabb_max, in vec4 q, in vec4 p)  {
    vec4 r = q - p;
    vec3 rmax = aabb_max - p.xyz;
    vec3 rmin = aabb_min - p.xyz;

    const float eps = 0.0000001;

    if (r.x > rmax.x + eps) {
        r *= (rmax.x / r.x);
    }
    if (r.y > rmax.y + eps) {
        r *= (rmax.y / r.y);
    }
    if (r.z > rmax.z + eps) {
        r *= (rmax.z / r.z);
    }

    if (r.x < rmin.x - eps) {
        r *= (rmin.x / r.x);
    }
    if (r.y < rmin.y - eps) {
        r *= (rmin.y / r.y);
    }
    if (r.z < rmin.z - eps) {
        r *= (rmin.z / r.z);
    }

    return p + r;
}

float compute_luminance(in vec3 color) {
    return dot(color, vec3(0.2127, 0.7152, 0.0722));
}

void main() {
    vec3 source_sample_total = vec3(0);
    float source_sample_weight = 0;
    vec3 neighborhood_min = vec3(10000);
    vec3 neighborhood_max = vec3(-10000);
    vec3 m1 = vec3(0);
    vec3 m2 = vec3(0);
    float closest_depth = 1.0;
    ivec2 closest_depth_pixel = ivec2(0);
    const ivec2 resolution = textureSize(u_final_color, 0);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            ivec2 pixel_position = ivec2(gl_FragCoord) + ivec2(x, y);
            pixel_position = clamp(pixel_position, ivec2(0), resolution);

            const vec3 neighbor = max(vec3(0), texelFetch(u_final_color, pixel_position, 0).rgb);
            const float sub_sample_distance = length(vec2(x, y));
            const float sub_sample_weight = compute_mitchell(sub_sample_distance, 1 / 3.0, 1 / 3.0);

            source_sample_total += neighbor * sub_sample_weight;
            source_sample_weight += sub_sample_weight;

            neighborhood_min = min(neighborhood_min, neighbor);
            neighborhood_max = max(neighborhood_max, neighbor);

            m1 += neighbor;
            m2 += neighbor * neighbor;

            const float current_depth = texelFetch(u_scene_depth, pixel_position, 0).r;
            if (current_depth < closest_depth) {
                closest_depth = current_depth;
                closest_depth_pixel = pixel_position;
            }
        }
    }

    const vec2 motion_vector = texelFetch(u_taa_velocity, closest_depth_pixel, 0).xy * vec2(0.5, -0.5);
    const vec2 history_uv = i_uvs - motion_vector;
    const vec3 source_sample = source_sample_total / source_sample_weight;

    if (history_uv.x != clamp(history_uv.x, 0.0, 1.0) ||
        history_uv.y != clamp(history_uv.y, 0.0, 1.0) ||
        ignore_history == 1)  {
        o_pixel = vec4(source_sample, 1);
    } else {
        vec3 history_sample = sample_catmull_rom(u_taa_history, history_uv).rgb;
        const float one_over_samples = 1.0 / 9.0;
        const float gamma = 1.0;
        const vec3 mu = m1 * one_over_samples;
        const vec3 sigma = sqrt(abs((m2 * one_over_samples) - (mu * mu)));
        const vec3 minc = mu - gamma * sigma;
        const vec3 maxc = mu + gamma * sigma;
        const vec3 clamped_sample = clamp(history_sample, neighborhood_min, neighborhood_max);
        history_sample = compute_aabb_clip(minc, maxc, vec4(clamped_sample, 1.0), vec4(clamped_sample, 1.0)).rgb;

        float source_weight = 0.05;
        float history_weight = 1.0 - source_weight;
        const vec3 compressed_source = source_sample * (1 / (max(max(source_sample.r, source_sample.g), source_sample.b) + 1.0));
        const vec3 compressed_history = history_sample * (1 / (max(max(history_sample.r, history_sample.g), history_sample.b) + 1.0));
        const float luminance_source = compute_luminance(compressed_source);
        const float luminance_history = compute_luminance(compressed_history);

        source_weight *= 1.0 / (1.0 + luminance_source);
        history_weight *= 1.0 / (1.0 + luminance_history);

        const vec3 result = (source_sample * source_weight + history_sample * history_weight) / max(source_weight + history_weight, 0.00001);
        o_pixel = vec4(result, 1);
    }
}
