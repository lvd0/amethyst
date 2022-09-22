#ifndef AM_GLSL_COMMON
#define AM_GLSL_COMMON

#if defined(__cplusplus)
    #define mat4 glm::mat4
    #define vec4 glm::vec4
    #define vec3 glm::vec3

    #define am_glsl_uint32 std::uint32_t
#else
    #define am_glsl_uint32 uint
#endif

#if !defined(__cplusplus)
    struct SVertexData {
        vec3 position;
        vec3 normal;
        vec2 uv;
        vec4 tangent;
    };
#endif

struct SCameraData {
    mat4 projection;
    mat4 view;
    mat4 proj_view;
    vec4 position;
};

struct STransformData {
    mat4 transform;
};

struct SPointLight {
    vec3 position;
    float _pad0;
    vec3 diffuse;
    float _pad1;
    vec3 specular;
    float _pad2;
    float radius;
    float _pad3[3];
};

struct SDirectionalLight {
    vec3 direction;
    float _pad0;
    vec3 diffuse;
    float _pad1;
    vec3 specular;
    float _pad2;
};

struct SObjectData {
    am_glsl_uint32 transform_index[2];
    am_glsl_uint32 albedo_index;
    am_glsl_uint32 normal_index;
    am_glsl_uint32 specular_index;
};

#if !defined(__cplusplus)
    #define AMBIENT_FACTOR 0.033
#endif

#if defined(__cplusplus)
    #undef mat4
    #undef vec4
    #undef vec3
#endif

#undef am_glsl_uint32

#endif
