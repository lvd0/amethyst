#ifndef AM_GLSL_COMMON
#define AM_GLSL_COMMON

#define AM_GLSL_MAX_CASCADES 4

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

#if defined(__cplusplus)
struct alignas(16) SCameraData {
#else
struct SCameraData {
#endif
    mat4 projection;
    mat4 view;
    mat4 proj_view;
    vec4 position;
    vec4 frustum[6];
    float p_near;
    float p_far;
};

#if defined(__cplusplus)
struct alignas(16) SShadowCascade {
#else
struct SShadowCascade {
#endif
    mat4 projection;
    mat4 view;
    mat4 proj_view;
    float split;
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

struct SGLSLDrawCommandIndirect {
    am_glsl_uint32 indices;
    am_glsl_uint32 instances;
    am_glsl_uint32 first_index;
    am_glsl_int32 vertex_offset;
    am_glsl_uint32 first_instance;
};

struct SAABB {
    vec4 center;
    vec4 extents;
    vec4 min;
    vec4 max;
};

struct SGLSLMaterial {
    vec4 base_color;
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
    am_glsl_uint32 indirect_offset;
    SGLSLDrawCommandIndirect indirect_data;
    SGLSLMaterial material;
    SAABB aabb;
};

#if !defined(__cplusplus)
    #define AMBIENT_FACTOR 0.033
    #define SHADOW_AMBIENT_FACTOR AMBIENT_FACTOR
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
