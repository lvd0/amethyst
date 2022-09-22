#ifndef AM_GLSL_COMMON
#define AM_GLSL_COMMON

#if defined(__cplusplus)
    #define mat4 glm::mat4
    #define vec4 glm::vec4
    #define vec3 glm::vec3

    #define am_glsl_int32 std::int32_t
    #define am_glsl_uint32 std::uint32_t
    #define am_glsl_uint64 std::uint64_t
#else
    #extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
    #define am_glsl_int32 int
    #define am_glsl_uint32 uint
    #define am_glsl_uint64 uint64_t
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
    mat4 current;
    mat4 previous;
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
    am_glsl_uint64 vertex_address;
    am_glsl_uint64 index_address;
    am_glsl_int32 vertex_offset;
    am_glsl_uint32 index_offset;
};

#if !defined(__cplusplus)
    #define AMBIENT_FACTOR 0.033
#endif

#if defined(__cplusplus)
    #undef mat4
    #undef vec4
    #undef vec3
#endif

#undef am_glsl_int32
#undef am_glsl_uint32
#undef am_glsl_uint64

#endif
