#include <amethyst/graphics/descriptor_pool.hpp>
#include <amethyst/graphics/command_buffer.hpp>
#include <amethyst/graphics/descriptor_set.hpp>
#include <amethyst/graphics/typed_buffer.hpp>
#include <amethyst/graphics/render_pass.hpp>
#include <amethyst/graphics/async_model.hpp>
#include <amethyst/graphics/framebuffer.hpp>
#include <amethyst/graphics/ui_context.hpp>
#include <amethyst/graphics/query_pool.hpp>
#include <amethyst/graphics/swapchain.hpp>
#include <amethyst/graphics/pipeline.hpp>
#include <amethyst/graphics/context.hpp>
#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/image.hpp>
#include <amethyst/graphics/queue.hpp>

#include <amethyst/meta/constants.hpp>
#include <amethyst/meta/enums.hpp>
#include <amethyst/meta/hash.hpp>

#include <amethyst/window/windowing_system.hpp>
#include <amethyst/window/window.hpp>
#include <amethyst/window/input.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include <unordered_map>
#include <unordered_set>
#include <numeric>
#include <vector>
#include <deque>

#include <imgui.h>
#include <implot.h>

#include "shadows/common.glsl"
#include "common.hpp"

namespace am::tst {
    struct SSubMesh {
        const STexturedMesh* mesh;
        uint32 textures[3];
        uint32 transform[2];
        uint32 instances;
    };

    struct SScene {
        using MeshBuffer = std::pair<const CRawBuffer*, const CRawBuffer*>;
        std::vector<STextureInfo> textures;
        std::vector<STransformData> local_transforms;
        std::vector<STransformData> world_transforms;
        std::unordered_map<MeshBuffer, std::vector<SSubMesh>> meshes;
        std::unordered_map<const STexturedMesh*, std::array<uint32, 3>> submesh_history;
    };

    struct SRotation {
        glm::vec3 axis;
        float32 angle;
    };

    struct STransform {
        glm::vec3 position;
        SRotation rotation;
        glm::vec3 scale;
    };

    struct SDraw {
        CAsyncModel* model;
        std::vector<STransform> transform;
    };

    static inline SScene build_scene(CDevice* device, const std::vector<SDraw>& draws, CAsyncTexture* fallback, const SScene& old_scene) noexcept {
        AM_PROFILE_SCOPED();
        constexpr auto sampler = SSamplerInfo {
            .filter = EFilter::Linear,
            .mip_mode = EMipMode::Linear,
            .border_color = EBorderColor::FloatOpaqueBlack,
            .address_mode = EAddressMode::Repeat,
            .anisotropy = 16,
        };
        SScene scene;
        scene.textures.reserve(1024);
        scene.textures = { device->sample(fallback, sampler) };
        scene.local_transforms.reserve(4096);
        scene.world_transforms.reserve(1024);
        std::unordered_map<const void*, uint32> texture_cache;
        uint32 local_offset = 0;
        uint32 world_offset = 0;
        for (uint32 i = 0; const auto& [model, transforms] : draws) {
            if (!model->is_ready()) { continue; }
            uint32 old_world_offset = 0;
            uint32 old_instance_size = 0;
            for (uint32 j = 0; const auto& submesh : model->submeshes()) {
                if (!submesh.geometry->is_ready()) { continue; }
                const auto mesh_buffer = SScene::MeshBuffer {
                    submesh.geometry->vertices()->handle(),
                    submesh.geometry->indices()->handle()
                };
                auto& current = scene.meshes[mesh_buffer];
                std::array<uint32, 3> indices = {};
                const auto emplace_descriptor = [&](CAsyncTexture* texture, uint32 which) {
                    AM_LIKELY_IF(texture) {
                        auto& cached = texture_cache[texture];
                        AM_LIKELY_IF(cached == 0 && texture->is_ready()) {
                            scene.textures.emplace_back(device->sample(texture, sampler));
                            cached = (uint32)scene.textures.size() - 1;
                        }
                        indices[which] = cached;
                    }
                };
                emplace_descriptor(submesh.albedo.get(), 0);
                emplace_descriptor(submesh.normal.get(), 1);
                auto result = SSubMesh {
                    .mesh = &submesh,
                    .textures = { indices[0], indices[1] },
                    .transform = { local_offset++, world_offset },
                    .instances = (uint32)transforms.size(),
                };
                current.push_back(result);
                scene.submesh_history[result.mesh] = { {
                    result.transform[0],
                    result.transform[1],
                    (uint32)transforms.size()
                } };
                auto prev_transform = glm::mat4(1.0f);
                AM_LIKELY_IF(old_scene.submesh_history.contains(result.mesh)) {
                    const auto& old_mesh = old_scene.submesh_history.at(result.mesh);
                    prev_transform = old_scene.local_transforms[old_mesh[0]].current;
                    old_world_offset = old_mesh[1];
                    old_instance_size = old_mesh[2];
                }
                scene.local_transforms.push_back({
                    .current = submesh.transform,
                    .previous = prev_transform
                });
                j++;
            }
            for (uint32 index = 0; const auto& transform : transforms) {
                auto result = glm::translate(glm::mat4(1.0f), transform.position);
                result = glm::scale(result, transform.scale);
                AM_LIKELY_IF(std::fpclassify(transform.rotation.angle) != FP_ZERO) {
                    result = glm::rotate(result, glm::radians(transform.rotation.angle), transform.rotation.axis);
                }
                auto prev_transform = glm::mat4(1.0f);
                AM_LIKELY_IF(index < old_instance_size) {
                    prev_transform = old_scene.world_transforms[old_world_offset + index].current;
                }
                scene.world_transforms.push_back({
                    .current = result,
                    .previous = prev_transform
                });
                index++;
            }
            world_offset += (uint32)transforms.size();
            i++;
        }
        return scene;
    }

    static inline std::array<SShadowCascade, AM_GLSL_MAX_CASCADES> compute_cascades(const CCamera& camera, glm::vec3 light_pos) noexcept {
        AM_PROFILE_SCOPED();
        std::array<SShadowCascade, AM_GLSL_MAX_CASCADES> cascades = {};
        const auto compute_cascade = [&camera, &light_pos](float32 p_near, float32 p_far) -> std::array<glm::mat4, 2> {
            glm::vec4 corners[8];
            { // Calculate frustum corners
                const auto perspective = glm::perspective(glm::radians(CCamera::fov), camera.aspect(), p_near, p_far);
                const auto inverse = glm::inverse(perspective * camera.view());
                uint32 offset = 0;
                for (uint32 x = 0; x < 2; ++x) {
                    for (uint32 y = 0; y < 2; ++y) {
                        for (uint32 z = 0; z < 2; ++z) {
                            const auto v_world = inverse * glm::vec4(
                                2.0f * (float32)x - 1.0f,
                                2.0f * (float32)y - 1.0f,
                                2.0f * (float32)z - 1.0f,
                                1.0f);
                            corners[offset++] = v_world / v_world.w;
                        }
                    }
                }
            }
            glm::mat4 light_view;
            { // Calculate light view matrix
                auto center = glm::vec3(0.0f);
                for (const auto& corner : corners) {
                    center += glm::vec3(corner);
                }
                center /= 8.0f;
                light_pos.x += 0.01f;
                light_pos.z += 0.01f;
                const auto light_dir = glm::normalize(light_pos);
                const auto eye = center + light_dir;
                light_view = glm::lookAt(eye, center, { 0.0f, 1.0f, 0.0f });
            }

            glm::mat4 light_proj;
            {
                float32 min_x = (std::numeric_limits<float32>::max)();
                float32 max_x = (std::numeric_limits<float32>::min)();
                float32 min_y = min_x;
                float32 max_y = max_x;
                float32 min_z = min_x;
                float32 max_z = max_x;
                for (const auto& corner : corners) {
                    const auto trf = light_view * corner;
                    min_x = std::min(min_x, trf.x);
                    max_x = std::max(max_x, trf.x);
                    min_y = std::min(min_y, trf.y);
                    max_y = std::max(max_y, trf.y);
                    min_z = std::min(min_z, trf.z);
                    max_z = std::max(max_z, trf.z);
                }
                constexpr auto z_mult = 10.0f;
                if (min_z < 0) {
                    min_z *= z_mult;
                } else {
                    min_z /= z_mult;
                }
                if (max_z < 0) {
                    max_z /= z_mult;
                } else {
                    max_z *= z_mult;
                }

                light_proj = glm::ortho(min_x, max_x, min_y, max_y, min_z, max_z);
            }
            return { light_proj, light_view };
        };
        const float32 cascade_levels[AM_GLSL_MAX_CASCADES] = {
            CCamera::p_far / 25.0f,
            CCamera::p_far / 12.5f,
            CCamera::p_far / 5.0f,
            CCamera::p_far,
        };
        for (std::size_t i = 0; i < AM_GLSL_MAX_CASCADES; ++i) {
            float32 p_near;
            float32 p_far;
            if (i == 0) {
                p_near = CCamera::p_near;
                p_far = cascade_levels[i];
            } else {
                p_near = cascade_levels[i - 1];
                p_far = cascade_levels[i];
            }
            const auto [projection, view] = compute_cascade(p_near, p_far);
            cascades[i] = {
                projection,
                view,
                projection * view,
                p_far
            };
        }
        return cascades;
    }
} // namespace am::tst

static inline am::CPipeline::SGraphicsCreateInfo shadow_pipeline_info(const am::CFramebuffer* framebuffer) noexcept {
    AM_PROFILE_SCOPED();
    return {
        .vertex = "../data/shaders/shadows/shadow.vert",
        .fragment = {},
        .geometry = {},
        .attributes = {
            am::EVertexAttribute::Vec3,
            am::EVertexAttribute::Vec3,
            am::EVertexAttribute::Vec2,
            am::EVertexAttribute::Vec4
        },
        .attachments = {},
        .states = {
            am::EDynamicState::Viewport,
            am::EDynamicState::Scissor
        },
        .cull = am::ECullMode::Front,
        .depth_test = true,
        .depth_write = true,
        .subpass = 0,
        .framebuffer = framebuffer
    };
}

static inline am::CPipeline::SGraphicsCreateInfo visibility_pipeline_info(const am::CFramebuffer* framebuffer) noexcept {
    AM_PROFILE_SCOPED();
    return {
        .vertex = "../data/shaders/shadows/visibility.vert",
        .fragment = "../data/shaders/shadows/visibility.frag",
        .geometry = {},
        .attributes = {
            am::EVertexAttribute::Vec3,
            am::EVertexAttribute::Vec3,
            am::EVertexAttribute::Vec2,
            am::EVertexAttribute::Vec4
        },
        .attachments = {},
        .states = {
            am::EDynamicState::Viewport,
            am::EDynamicState::Scissor
        },
        .cull = am::ECullMode::Back,
        .depth_test = true,
        .depth_write = true,
        .subpass = 0,
        .framebuffer = framebuffer
    };
}

static inline am::CPipeline::SGraphicsCreateInfo final_pipeline_info(const am::CFramebuffer* framebuffer) noexcept {
    AM_PROFILE_SCOPED();
    return {
        .vertex = "../data/shaders/shadows/final.vert",
        .fragment = "../data/shaders/shadows/final.frag",
        .geometry = {},
        .attributes = {},
        .attachments = {},
        .states = {
            am::EDynamicState::Viewport,
            am::EDynamicState::Scissor
        },
        .cull = am::ECullMode::None,
        .depth_test = false,
        .depth_write = false,
        .subpass = 0,
        .framebuffer = framebuffer
    };
}

static inline am::CPipeline::SComputeCreateInfo cull_pipeline_info() noexcept {
    AM_PROFILE_SCOPED();
    return {
        .compute = "../data/shaders/shadows/cull.comp"
    };
}

static inline am::CPipeline::SComputeCreateInfo depth_reduce_pipeline_info() noexcept {
    AM_PROFILE_SCOPED();
    return {
        .compute = "../data/shaders/shadows/depth_reduce.comp"
    };
}

static inline am::uint32 previous_power_2(am::uint32 v) {
    AM_PROFILE_SCOPED();
    am::uint32 r = 1;
    while ((r << 1) < v) {
        r <<= 1;
    }
    return r;
}

static inline am::uint32 compute_mips(am::uint32 width, am::uint32 height) noexcept {
    AM_PROFILE_SCOPED();
    am::uint32 result = 1;
    while (width > 1 || height > 1) {
        width /= 2;
        height /= 2;
        result++;
    }
    return result;
}

struct SDepthPyramidData {
    am::CRcPtr<am::CImageView> view;
    am::CRcPtr<am::CDescriptorSet> set;
};

struct SEngineState {
    bool vsync = true;
    bool frustum_culling = true;
    bool occlusion_culling = true;
    glm::vec3 directional_light_position = { 0, 1000, 0 };
    std::vector<SPointLight> point_lights;
    std::deque<am::float64> delta_time_history;
    std::vector<am::uint64> pipeline_statistics;
};

class Application {
public:
    using Self = Application;

    Application() noexcept {
        AM_PROFILE_SCOPED();
        _system = am::CWindowingSystem::make();
        _window = am::CWindow::make({
            .width = (am::uint32)1600,
            .height = (am::uint32)900,
            .title = "Test Engine",
        });
        _input = am::CInput::make(_window);
        _context = am::CContext::make({
            .main_extensions = {
                am::EAPIExtension::ExtraValidation
            },
            .surface_extensions = _system->surface_extensions()
        });
        _device = am::CDevice::make(_context, {
            .extensions = {
                am::EDeviceExtension::Swapchain
            }
        });
        _swapchain = am::CSwapchain::make(_device, _window, {
            .vsync = _state.vsync,
            .srgb = false
        });
        _descriptor_pool = am::CDescriptorPool::make(_device);
        _shadow_pass = am::CRenderPass::make(_device, {
            .attachments = { {
                .format = { am::EResourceFormat::D32Sfloat },
                .samples = am::EImageSampleCount::s1,
                .layout = {
                    .initial = am::EImageLayout::Undefined,
                    .final = am::EImageLayout::ShaderReadOnlyOptimal
                },
                .clear = am::CClearValue::make({ .depth = 1 }),
            } },
            .subpasses = { {
                .attachments = { 0 },
                .preserve = {},
                .input = {}
            } },
            .dependencies = { {
                .source_subpass = am::external_subpass,
                .dest_subpass = 0,
                .source_stage = am::EPipelineStage::EarlyFragmentTests |
                                am::EPipelineStage::LateFragmentTests,
                .dest_stage = am::EPipelineStage::EarlyFragmentTests |
                              am::EPipelineStage::LateFragmentTests,
                .source_access = am::EResourceAccess::DepthStencilAttachmentWrite,
                .dest_access = am::EResourceAccess::DepthStencilAttachmentWrite
            }, {
                .source_subpass = 0,
                .dest_subpass = am::external_subpass,
                .source_stage = am::EPipelineStage::EarlyFragmentTests |
                                am::EPipelineStage::LateFragmentTests,
                .dest_stage = am::EPipelineStage::FragmentShader,
                .source_access = am::EResourceAccess::DepthStencilAttachmentWrite,
                .dest_access = am::EResourceAccess::ShaderRead
            } }
        });
        _visibility_pass = am::CRenderPass::make(_device, {
            .attachments = { {
                .format = { am::EResourceFormat::R32G32Uint },
                .samples = am::EImageSampleCount::s1,
                .layout = {
                    .initial = am::EImageLayout::Undefined,
                    .final = am::EImageLayout::General
                },
                .clear = am::CClearValue::make({ .u32 = {} })
            }, {
                .format = { am::EResourceFormat::D32Sfloat },
                .samples = am::EImageSampleCount::s1,
                .layout = {
                    .initial = am::EImageLayout::Undefined,
                    .final = am::EImageLayout::ShaderReadOnlyOptimal
                },
                .clear = am::CClearValue::make({ .depth = 1 }),
            } },
            .subpasses = { {
                .attachments = { 0, 1 },
                .preserve = {},
                .input = {}
            } },
            .dependencies = { {
                .source_subpass = am::external_subpass,
                .dest_subpass = 0,
                .source_stage = am::EPipelineStage::ColorAttachmentOutput |
                                am::EPipelineStage::EarlyFragmentTests |
                                am::EPipelineStage::LateFragmentTests,
                .dest_stage = am::EPipelineStage::ColorAttachmentOutput |
                              am::EPipelineStage::EarlyFragmentTests |
                              am::EPipelineStage::LateFragmentTests,
                .source_access = am::EResourceAccess::ColorAttachmentWrite |
                                 am::EResourceAccess::DepthStencilAttachmentWrite,
                .dest_access = am::EResourceAccess::ColorAttachmentWrite |
                               am::EResourceAccess::DepthStencilAttachmentWrite
            }, {
                .source_subpass = 0,
                .dest_subpass = am::external_subpass,
                .source_stage = am::EPipelineStage::ColorAttachmentOutput |
                                am::EPipelineStage::EarlyFragmentTests |
                                am::EPipelineStage::LateFragmentTests,
                .dest_stage = am::EPipelineStage::FragmentShader |
                              am::EPipelineStage::ComputeShader,
                .source_access = am::EResourceAccess::ColorAttachmentWrite |
                                 am::EResourceAccess::DepthStencilAttachmentWrite,
                .dest_access = am::EResourceAccess::ShaderRead
            } }
        });
        _final_pass = am::CRenderPass::make(_device, {
            .attachments = { {
                .format = { _swapchain->format() },
                .samples = am::EImageSampleCount::s1,
                .layout = {
                    .initial = am::EImageLayout::Undefined,
                    .final = am::EImageLayout::ShaderReadOnlyOptimal
                },
                .clear = am::CClearValue::make({ .u32 = { 0, 0, 0, 1 } })
            } },
            .subpasses = { {
                .attachments = { 0 },
                .preserve = {},
                .input = {}
            } },
            .dependencies = { {
                .source_subpass = am::external_subpass,
                .dest_subpass = 0,
                .source_stage = am::EPipelineStage::ColorAttachmentOutput,
                .dest_stage = am::EPipelineStage::ColorAttachmentOutput,
                .source_access = am::EResourceAccess::ColorAttachmentWrite,
                .dest_access = am::EResourceAccess::ColorAttachmentWrite
            }, {
                .source_subpass = 0,
                .dest_subpass = am::external_subpass,
                .source_stage = am::EPipelineStage::ColorAttachmentOutput,
                .dest_stage = am::EPipelineStage::FragmentShader,
                .source_access = am::EResourceAccess::ColorAttachmentWrite,
                .dest_access = am::EResourceAccess::ShaderRead
            } }
        });
        _shadow_framebuffer = am::CFramebuffer::make(_device, {
            .width = 2048,
            .height = 2048,
            .attachments = { {
                am::make_attachment({
                    .index = 0,
                    .usage = am::EImageUsage::DepthStencilAttachment |
                             am::EImageUsage::Sampled,
                    .layers = AM_GLSL_MAX_CASCADES
                })
            } },
            .pass = _shadow_pass
        });
        _visibility_framebuffer = am::CFramebuffer::make(_device, {
            .width = _swapchain->width(),
            .height = _swapchain->height(),
            .attachments = { {
                am::make_attachment({
                    .index = 0,
                    .usage = am::EImageUsage::ColorAttachment |
                             am::EImageUsage::Storage,
                }),
                am::make_attachment({
                    .index = 1,
                    .usage = am::EImageUsage::DepthStencilAttachment |
                             am::EImageUsage::Sampled,
                })
            } },
            .pass = _visibility_pass
        });
        _final_framebuffer = am::CFramebuffer::make(_device, {
            .width = _swapchain->width(),
            .height = _swapchain->height(),
            .attachments = { {
                am::make_attachment({
                    .index = 0,
                    .usage = am::EImageUsage::ColorAttachment |
                             am::EImageUsage::Sampled,
                })
            } },
            .pass = _final_pass
        });
        _viewport_size = {
            (am::float32)_swapchain->width(),
            (am::float32)_swapchain->height()
        };
        _ui_context = am::CUIContext::make({
            .device = _device,
            .window = _window,
            .swapchain = _swapchain,
            .load = false
        });

        _shadow_pipeline = am::CPipeline::make(_device, shadow_pipeline_info(_shadow_framebuffer.get()));
        _depth_reduce_pipeline = am::CPipeline::make(_device, depth_reduce_pipeline_info());
        _cull_pipeline = am::CPipeline::make(_device, cull_pipeline_info());
        _visibility_pipeline = am::CPipeline::make(_device, visibility_pipeline_info(_visibility_framebuffer.get()));
        _final_pipeline = am::CPipeline::make(_device, final_pipeline_info(_final_framebuffer.get()));
        _pipeline_statistics = am::CQueryPool::make(_device, {
            .statistics =
                am::EQueryPipelineStatistics::InputAssemblyVertices |
                am::EQueryPipelineStatistics::InputAssemblyPrimitives |
                am::EQueryPipelineStatistics::VertexShaderInvocations |
                am::EQueryPipelineStatistics::GeometryShaderInvocations |
                am::EQueryPipelineStatistics::GeometryShaderPrimitives |
                am::EQueryPipelineStatistics::ClippingInvocations |
                am::EQueryPipelineStatistics::ClippingPrimitives |
                am::EQueryPipelineStatistics::FragmentShaderInvocations |
                am::EQueryPipelineStatistics::ComputeShaderInvocations,
            .type = am::EQueryType::PipelineStatistics,
            .count = 9
        });
        _commands = am::CCommandBuffer::make(_device, am::frames_in_flight, {
            .queue = am::EQueueType::Graphics,
            .pool = am::ECommandPoolType::Main,
            .index = 0
        });

        _fences = am::CFence::make(_device, am::frames_in_flight);
        _image_acq = am::CSemaphore::make(_device, am::frames_in_flight);
        _graphics_done = am::CSemaphore::make(_device, am::frames_in_flight);

        _camera = am::tst::CCamera::make(_input.get());

        _models = {
            // am::CAsyncModel::make(_device, "../data/models/intel_sponza/intel_sponza.gltf"),
            // am::CAsyncModel::make(_device, "../data/models/intel_sponza/curtains/curtains.gltf"),
            // am::CAsyncModel::make(_device, "../data/models/sponza/Sponza.gltf"),
            // am::CAsyncModel::make(_device, "../data/models/cornell_box/scene.gltf"),
            // am::CAsyncModel::make(_device, "../data/models/cube/Cube.gltf"),
            // am::CAsyncModel::make(_device, "../data/models/deccer_cubes/SM_Deccer_Cubes_Textured.gltf"),
            // am::CAsyncModel::make(_device, "../data/models/agamemnon/scene.gltf"),
            am::CAsyncModel::make(_device, "../data/models/rock3/rock3.gltf"),
        };

        _draws = { {
            .model = _models[0].get(),
            .transform = { {
                .position = glm::vec3(-30.0f, 0.0f, 0.0f),
                .rotation = {
                    .axis = glm::vec3(1.0f, 0.0f, 0.0f),
                    .angle = 45.0f
                },
                .scale = glm::vec3(1.0f)
            }, {
                .position = {},
                .rotation = {
                    .axis = glm::vec3(0.0f, 1.0f, 0.0f),
                    .angle = 45.0f
                },
                .scale = glm::vec3(1.0f)
            }, {
                .position = glm::vec3(30.0f, 0.0f, 0.0f),
                .rotation = {
                    .axis = glm::vec3(0.0f, 0.0f, 1.0f),
                    .angle = 45.0f
                },
                .scale = glm::vec3(1.0f)
            } }
        } };

        _default_texture = am::CAsyncTexture::sync_make(_device, {
            .path = "../data/textures/default.ktx2",
            .type = am::ETextureType::Color
        });

        _shadow_set = am::CDescriptorSet::make(_device, am::frames_in_flight, {
            .pool = _descriptor_pool,
            .pipeline = _shadow_pipeline,
            .index = 0
        });
        _cull_set = am::CDescriptorSet::make(_device, am::frames_in_flight, {
            .pool = _descriptor_pool,
            .pipeline = _cull_pipeline,
            .index = 0
        });
        _visibility_set = am::CDescriptorSet::make(_device, am::frames_in_flight, {
            .pool = _descriptor_pool,
            .pipeline = _visibility_pipeline,
            .index = 0
        });
        _final_set = am::CDescriptorSet::make(_device, am::frames_in_flight, {
            .pool = _descriptor_pool,
            .pipeline = _final_pipeline,
            .index = 0
        });
        _light_set = am::CDescriptorSet::make(_device, am::frames_in_flight, {
            .pool = _descriptor_pool,
            .pipeline = _final_pipeline,
            .index = 1
        });
        _ui_set = am::CDescriptorSet::make(_device, {
            .pool = _descriptor_pool,
            .layout = _ui_context->set_layout(),
            .index = 0
        });
        _make_depth_pyramid(_swapchain->width(), _swapchain->height());
        _shadow_cascade_uniform = am::CTypedBuffer<SShadowCascade>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::UniformBuffer,
            .memory = am::memory_auto,
            .capacity = AM_GLSL_MAX_CASCADES
        });
        _camera_uniform = am::CTypedBuffer<SCameraData>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::UniformBuffer,
            .memory = am::memory_auto,
            .capacity = 2
        });
        _local_transform_storage = am::CTypedBuffer<STransformData>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 64
        });
        _world_transform_storage = am::CTypedBuffer<STransformData>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 64
        });
        _point_light_storage = am::CTypedBuffer<SPointLight>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 64
        });
        _directional_light_storage = am::CTypedBuffer<SDirectionalLight>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 64
        });
        _object_storage = am::CTypedBuffer<SObjectData>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 64
        });
        _shadow_indirect_commands = am::CTypedBuffer<am::SDrawCommandIndexedIndirect>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::IndirectBuffer,
            .memory = am::memory_auto,
            .capacity = 16384
        });
        _indirect_commands = am::CTypedBuffer<am::SDrawCommandIndexedIndirect>::make(_device, {
            .usage = am::EBufferUsage::StorageBuffer | am::EBufferUsage::IndirectBuffer,
            .memory = { am::EMemoryProperty::DeviceLocal },
            .capacity = 16384
        });
        _object_offset_storage = am::CTypedBuffer<am::uint32>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 16384
        });
        _instance_offset_storage = am::CTypedBuffer<am::uint32>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 16384
        });
        _draw_count_storage = am::CTypedBuffer<am::uint32>::make(_device, {
            .usage = am::EBufferUsage::StorageBuffer | am::EBufferUsage::IndirectBuffer,
            .memory = { am::EMemoryProperty::DeviceLocal },
            .capacity = 16384
        });
        _object_remap_storage = am::CTypedBuffer<am::uint32>::make(_device, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = { am::EMemoryProperty::DeviceLocal },
            .capacity = 16384
        });
        _instance_remap_storage = am::CTypedBuffer<am::uint32>::make(_device, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = { am::EMemoryProperty::DeviceLocal },
            .capacity = 16384
        });

        _state.delta_time_history.resize(512);
    }

    ~Application() noexcept { // Hopefully this destroys everything in the correct order
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(_device->logger(), "terminating program");
        _device->wait_idle();
    }

    void update() noexcept {
        AM_PROFILE_SCOPED();
        const auto current_time = _system->current_time();
        _delta_time = current_time - _last_time;
        _last_time = current_time;
        _frames += _delta_time;
        _state.delta_time_history.pop_front();
        _state.delta_time_history.push_back(_delta_time * 1000);

        _system->poll_events();
        while (_window->width() == 0 || _window->height() == 0) {
            _system->wait_events();
            _window->update_viewport();
        }
        _input->capture();
        _camera.update({ _viewport_size.x, _viewport_size.y }, (am::float32)_delta_time);
        _scene = build_scene(_device.get(), _draws, _default_texture.get(), _scene);
        const auto cascades = am::tst::compute_cascades(_camera, _state.directional_light_position);
        _fences[_frame_index]->wait_and_reset();
        _build_object_data();

        if (_input->is_key_pressed_once(am::Keyboard::kR)) {
            _device->cleanup_after(
                am::frames_in_flight + 1,
                [p0 = std::move(_cull_pipeline),
                 p1 = std::move(_visibility_pipeline),
                 p2 = std::move(_final_pipeline)](const am::CDevice*) mutable noexcept {});
            _shadow_pipeline = am::CPipeline::make(_device, shadow_pipeline_info(_shadow_framebuffer.get()));
            _cull_pipeline = am::CPipeline::make(_device, cull_pipeline_info());
            _visibility_pipeline = am::CPipeline::make(_device, visibility_pipeline_info(_visibility_framebuffer.get()));
            _final_pipeline = am::CPipeline::make(_device, final_pipeline_info(_final_framebuffer.get()));

            for (am::uint32 i = 0; i < am::frames_in_flight; ++i) {
                _shadow_set[i]->update_pipeline(_shadow_pipeline);
                _cull_set[i]->update_pipeline(_cull_pipeline);
                _visibility_set[i]->update_pipeline(_visibility_pipeline);
                _final_set[i]->update_pipeline(_final_pipeline);
                _light_set[i]->update_pipeline(_final_pipeline);
            }
        }

        SCameraData camera_buffer[2] = {};
        camera_buffer[0].projection = _camera.projection();
        camera_buffer[0].view = _camera.view();
        camera_buffer[0].proj_view = camera_buffer[0].projection * camera_buffer[0].view;
        camera_buffer[0].position = _camera.position();
        std::memcpy(&camera_buffer[0].frustum, &_camera.frustum(), sizeof camera_buffer[0].frustum);
        camera_buffer[0].p_near = am::tst::CCamera::p_near;
        camera_buffer[0].p_far = am::tst::CCamera::p_far;
        camera_buffer[1] = _old_camera;
        _old_camera = camera_buffer[0];

        _shadow_cascade_uniform[_frame_index]->insert(cascades.data(), am::size_bytes(cascades));
        _camera_uniform[_frame_index]->insert(camera_buffer, sizeof camera_buffer);

        _local_transform_storage[_frame_index]->insert(_scene.local_transforms);
        _world_transform_storage[_frame_index]->insert(_scene.world_transforms);
        _point_light_storage[_frame_index]->insert(_state.point_lights);
        _directional_light_storage[_frame_index]->insert({ {
            .direction = glm::normalize(_state.directional_light_position),
            .diffuse = glm::vec3(1.0f),
            .specular = glm::vec3(1.0f)
        } });

        _image_index = _device->acquire_image(_swapchain.get(), _image_acq[_frame_index].get());
        AM_UNLIKELY_IF(_swapchain->is_lost()) {
            _resize_resources();
        }

        if (_visibility_framebuffer->resize((am::uint32)_viewport_size.x, (am::uint32)_viewport_size.y)) {
            _final_framebuffer->resize((am::uint32)_viewport_size.x, (am::uint32)_viewport_size.y);
            _make_depth_pyramid((am::uint32)_viewport_size.x, (am::uint32)_viewport_size.y);
            _occlusion_cull = false;
        }

        _ui_context->acquire_frame();
        auto depth_sampler = am::SSamplerInfo {
            .filter = am::EFilter::Nearest,
            .mip_mode = am::EMipMode::Nearest,
            .border_color = am::EBorderColor::FloatOpaqueBlack,
            .address_mode = am::EAddressMode::ClampToEdge,
        };
        {
            for (am::uint32 i = 0; i < _depth_pyramid_data.size(); ++i) {
                am::STextureInfo depth_source = {};
                if (i == 0) {
                    depth_sampler.layout = am::EImageLayout::ShaderReadOnlyOptimal;
                    depth_source = _device->sample(_depth_pyramid_data[i].view.get(), depth_sampler);
                } else {
                    depth_sampler.layout = am::EImageLayout::General;
                    depth_source = _device->sample(_depth_pyramid_data[i].view.get(), depth_sampler);
                }
                _depth_pyramid_data[i].set->bind("u_scene_depth", depth_source);
            }
            for (am::uint32 i = 0; i < _depth_pyramid_views.size(); ++i) {
                _depth_pyramid_data[i].set->bind("u_depth_pyramid", _depth_pyramid_views[i].get());
            }
        }

        _shadow_set[_frame_index]->bind("BLocalTransforms", _local_transform_storage[_frame_index]->info());
        _shadow_set[_frame_index]->bind("BWorldTransforms", _world_transform_storage[_frame_index]->info());
        _shadow_set[_frame_index]->bind("BObjectData", _object_storage[_frame_index]->info());
        _shadow_set[_frame_index]->bind("UShadowCascades", _shadow_cascade_uniform[_frame_index]->info());

        _cull_set[_frame_index]->bind("BObjectData", _object_storage[_frame_index]->info());
        _cull_set[_frame_index]->bind("UCamera", _camera_uniform[_frame_index]->info());
        _cull_set[_frame_index]->bind("BLocalTransforms", _local_transform_storage[_frame_index]->info());
        _cull_set[_frame_index]->bind("BWorldTransforms", _world_transform_storage[_frame_index]->info());
        _cull_set[_frame_index]->bind("BCullingOutput", _indirect_commands->info());
        _cull_set[_frame_index]->bind("BDrawCountOutput", _draw_count_storage->info());
        _cull_set[_frame_index]->bind("BObjectOffsets", _object_offset_storage[_frame_index]->info());
        _cull_set[_frame_index]->bind("BObjectIDRemap", _object_remap_storage->info());
        _cull_set[_frame_index]->bind("BInstanceOffsets", _instance_offset_storage[_frame_index]->info());
        _cull_set[_frame_index]->bind("BInstanceIDRemap", _instance_remap_storage->info());
        _cull_set[_frame_index]->bind("u_depth_pyramid", _device->sample(_depth_pyramid.get(), depth_sampler));

        _visibility_set[_frame_index]->bind("UCamera", _camera_uniform[_frame_index]->info());
        _visibility_set[_frame_index]->bind("BLocalTransforms", _local_transform_storage[_frame_index]->info());
        _visibility_set[_frame_index]->bind("BWorldTransforms", _world_transform_storage[_frame_index]->info());
        _visibility_set[_frame_index]->bind("BObjectData", _object_storage[_frame_index]->info());
        _visibility_set[_frame_index]->bind("BObjectOffsets", _object_offset_storage[_frame_index]->info());
        _visibility_set[_frame_index]->bind("BObjectIDRemap", _object_remap_storage->info());
        _visibility_set[_frame_index]->bind("BInstanceOffsets", _instance_offset_storage[_frame_index]->info());
        _visibility_set[_frame_index]->bind("BInstanceIDRemap", _instance_remap_storage->info());

        _final_set[_frame_index]->bind("UCamera", _camera_uniform[_frame_index]->info());
        _final_set[_frame_index]->bind("BObjectData", _object_storage[_frame_index]->info());
        _final_set[_frame_index]->bind("BLocalTransforms", _local_transform_storage[_frame_index]->info());
        _final_set[_frame_index]->bind("BWorldTransforms", _world_transform_storage[_frame_index]->info());
        _final_set[_frame_index]->bind("u_visibility", _visibility_framebuffer->image(0));
        _final_set[_frame_index]->bind("u_textures", _scene.textures);

        _light_set[_frame_index]->bind("BPointLights", _point_light_storage[_frame_index]->info());
        _light_set[_frame_index]->bind("BDirectionalLights", _directional_light_storage[_frame_index]->info());
        _light_set[_frame_index]->bind("UShadowCascades", _shadow_cascade_uniform[_frame_index]->info());
        _light_set[_frame_index]->bind("u_shadow_map", _device->sample(_shadow_framebuffer->image(0), {
            .filter = am::EFilter::Nearest,
            .mip_mode = am::EMipMode::Nearest,
            .border_color = am::EBorderColor::FloatOpaqueWhite,
            .address_mode = am::EAddressMode::ClampToBorder
        }));
    }

    void render() noexcept {
        AM_PROFILE_SCOPED();
        auto& commands = *_commands[_frame_index];
        const auto draw_count_size = (am::uint32)_draw_count_storage->size();
        commands
            .begin()
            .begin_query(_pipeline_statistics.get(), 0)
            .begin_render_pass(_shadow_framebuffer.get())
            .bind_pipeline(_shadow_pipeline.get())
            .bind_descriptor_set(_shadow_set[_frame_index].get())
            .set_viewport(am::inverted_viewport_tag)
            .set_scissor();
        {
            for (am::uint32 layer = 0; layer < AM_GLSL_MAX_CASCADES; ++layer) {
                am::uint32 offset = 0;
                for (am::uint32 index = 0; const auto& [mesh_buffer, meshes] : _scene.meshes) {
                    const auto& [vertex_buffer, index_buffer] = mesh_buffer;
                    const am::uint32 constants[] = { offset, layer };
                    commands
                        .bind_vertex_buffer(vertex_buffer->info())
                        .bind_index_buffer(index_buffer->info())
                        .push_constants(am::EShaderStage::Vertex, constants, sizeof constants)
                        .draw_indexed_indirect(_shadow_indirect_commands[_frame_index]->info(offset), meshes.size());
                    offset += meshes.size();
                }
            }
        }
        commands.end_render_pass();
        if (_occlusion_cull && _state.occlusion_culling) {
            for (am::uint32 i = 0; i < _depth_pyramid->mips(); ++i) {
                const am::uint32 constants[] = {
                    std::max(_depth_pyramid->width() >> i, 1u),
                    std::max(_depth_pyramid->height() >> i, 1u)
                };
                commands
                    .bind_pipeline(_depth_reduce_pipeline.get())
                    .bind_descriptor_set(_depth_pyramid_data[i].set.get())
                    .push_constants(am::EShaderStage::Compute, constants, sizeof constants)
                    .dispatch(
                        (constants[0] + 16 - 1) / 16,
                        (constants[1] + 16 - 1) / 16)
                    .barrier({
                        .image = _depth_pyramid.get(),
                        .source_stage = am::EPipelineStage::ComputeShader,
                        .dest_stage = am::EPipelineStage::ComputeShader,
                        .source_access = am::EResourceAccess::ShaderWrite,
                        .dest_access = am::EResourceAccess::ShaderRead,
                        .old_layout = am::EImageLayout::General,
                        .new_layout = am::EImageLayout::General,
                        .mip = i
                    });
            }
        }
        const am::uint32 cull_constants[] = {
            draw_count_size,
            _state.frustum_culling,
            _occlusion_cull && _state.occlusion_culling
        };
        commands
            .bind_pipeline(_cull_pipeline.get())
            .bind_descriptor_set(_cull_set[_frame_index].get())
            .push_constants(am::EShaderStage::Compute, cull_constants, sizeof cull_constants)
            .dispatch((_object_storage[_frame_index]->size() / 256) + 1)
            .barrier({
                .buffer = _indirect_commands->info(),
                .source_stage = am::EPipelineStage::ComputeShader,
                .dest_stage = am::EPipelineStage::DrawIndirect,
                .source_access = am::EResourceAccess::ShaderWrite,
                .dest_access = am::EResourceAccess::IndirectCommandRead,
            })
            .barrier({
                .buffer = _draw_count_storage->info(),
                .source_stage = am::EPipelineStage::ComputeShader,
                .dest_stage = am::EPipelineStage::DrawIndirect |
                              am::EPipelineStage::Host,
                .source_access = am::EResourceAccess::ShaderWrite,
                .dest_access = am::EResourceAccess::IndirectCommandRead |
                               am::EResourceAccess::HostRead,
            })
            .barrier({
                .buffer = _object_remap_storage->info(),
                .source_stage = am::EPipelineStage::ComputeShader,
                .dest_stage = am::EPipelineStage::VertexShader |
                              am::EPipelineStage::FragmentShader,
                .source_access = am::EResourceAccess::ShaderWrite,
                .dest_access = am::EResourceAccess::ShaderRead,
            })
            .barrier({
                .buffer = _instance_remap_storage->info(),
                .source_stage = am::EPipelineStage::ComputeShader,
                .dest_stage = am::EPipelineStage::VertexShader |
                              am::EPipelineStage::FragmentShader,
                .source_access = am::EResourceAccess::ShaderWrite,
                .dest_access = am::EResourceAccess::ShaderRead,
            })
            .begin_render_pass(_visibility_framebuffer.get())
            .bind_pipeline(_visibility_pipeline.get())
            .bind_descriptor_set(_visibility_set[_frame_index].get())
            .set_viewport(am::inverted_viewport_tag)
            .set_scissor();
        {
            am::uint32 offset = 0;
            for (am::uint32 index = 0; const auto& [mesh_buffer, meshes] : _scene.meshes) {
                const auto& [vertex_buffer, index_buffer] = mesh_buffer;
                commands
                    .bind_vertex_buffer(vertex_buffer->info())
                    .bind_index_buffer(index_buffer->info())
                    .push_constants(am::EShaderStage::Vertex, &index, sizeof index)
                    .draw_indexed_indirect_count(
                        _indirect_commands->info(offset),
                        _draw_count_storage->info(index),
                        meshes.size() + 1);
                offset += meshes.size();
                index++;
            }
        }
        AM_UNLIKELY_IF(!_occlusion_cull) {
            _occlusion_cull = true;
        }
        const auto object_count = (am::uint32)_object_storage[_frame_index]->size();
        commands
            .end_render_pass()
            .begin_render_pass(_final_framebuffer.get())
            .bind_pipeline(_final_pipeline.get())
            .bind_descriptor_set(_final_set[_frame_index].get())
            .bind_descriptor_set(_light_set[_frame_index].get())
            .set_viewport(am::inverted_viewport_tag)
            .set_scissor()
            .push_constants(am::EShaderStage::Fragment, &object_count, sizeof object_count)
            .draw(3, 1, 0, 0)
            .end_render_pass();
        _draw_ui(commands);
        commands
            .transition_layout({
                .image = _swapchain->image(_image_index),
                .source_stage = am::EPipelineStage::TopOfPipe,
                .dest_stage = am::EPipelineStage::Transfer,
                .source_access = am::EResourceAccess::None,
                .dest_access = am::EResourceAccess::TransferWrite,
                .old_layout = am::EImageLayout::Undefined,
                .new_layout = am::EImageLayout::TransferDSTOptimal
            })
            .copy_image(_ui_context->framebuffer()->image(0), _swapchain->image(_image_index))
            .transition_layout({
                .image = _swapchain->image(_image_index),
                .source_stage = am::EPipelineStage::Transfer,
                .dest_stage = am::EPipelineStage::BottomOfPipe,
                .source_access = am::EResourceAccess::TransferWrite,
                .dest_access = am::EResourceAccess::None,
                .old_layout = am::EImageLayout::TransferDSTOptimal,
                .new_layout = am::EImageLayout::PresentSRC
            })
            .end_query(_pipeline_statistics.get(), 0)
            .end();

        _device->graphics_queue()->submit({ {
            .stage_mask = am::EPipelineStage::Transfer,
            .command = &commands,
            .wait = _image_acq[_frame_index].get(),
            .signal = _graphics_done[_frame_index].get(),
        } }, _fences[_frame_index].get());

        _device->graphics_queue()->present({
            .image = _image_index,
            .swapchain = _swapchain.get(),
            .wait = _graphics_done[_frame_index].get(),
        });

        _store_pipeline_statistics();
        AM_UNLIKELY_IF(_swapchain->is_lost()) {
            _resize_resources();
        }
        _device->update_cleanup();
        _frame_index = (_frame_index + 1) % am::frames_in_flight;
        _frame_count++;
        AM_MARK_FRAME();
    }

    void run() noexcept {
        AM_PROFILE_SCOPED();
        while (_window->is_open()) {
            update();
            render();
        }
    }

private:
    void _resize_resources() noexcept {
        AM_PROFILE_SCOPED();
        _swapchain->recreate({ .vsync = _state.vsync, .srgb = false });
        _ui_context->resize_framebuffer(_swapchain->width(), _swapchain->height());
    }

    void _build_object_data() noexcept {
        AM_PROFILE_SCOPED();
        std::vector<SObjectData> object_data;
        object_data.reserve(1024);
        am::uint32 offset = 0;
        am::uint32 instances = 0;
        _object_offset_storage[_frame_index]->clear();
        _instance_offset_storage[_frame_index]->clear();
        _shadow_indirect_commands[_frame_index]->clear();
        for (am::uint32 index = 0; const auto& [mesh_buffer, meshes] : _scene.meshes) {
            const auto& [vertex_buffer, index_buffer] = mesh_buffer;
            for (const auto& each : meshes) {
                const auto& geometry = each.mesh->geometry;
                object_data.push_back(SObjectData {
                    .transform_index = { each.transform[0], each.transform[1] },
                    .albedo_index = each.textures[0],
                    .normal_index = each.textures[1],
                    .specular_index = each.textures[2],
                    .vertex_address = vertex_buffer->address(),
                    .index_address = index_buffer->address(),
                    .vertex_offset = (am::int32)geometry->vertex_offset(),
                    .index_offset = (am::uint32)geometry->index_offset() / 3,
                    .indirect_offset = index,
                    .indirect_data = {
                        .indices = each.mesh->indices,
                        .instances = each.instances,
                        .first_index = (am::uint32)geometry->index_offset(),
                        .vertex_offset = (am::int32)geometry->vertex_offset(),
                        .first_instance = 0
                    },
                    .material = {
                        .base_color = each.mesh->material.base_color
                    },
                    .aabb = {
                        glm::make_vec4(each.mesh->aabb.center),
                        glm::make_vec4(each.mesh->aabb.extents),
                        glm::make_vec4(each.mesh->aabb.min),
                        glm::make_vec4(each.mesh->aabb.max),
                    }
                });
                _instance_offset_storage[_frame_index]->push_back(instances);
                _shadow_indirect_commands[_frame_index]->push_back({
                    .indices = each.mesh->indices,
                    .instances = each.instances,
                    .first_index = (am::uint32)geometry->index_offset(),
                    .vertex_offset = (am::int32)geometry->vertex_offset(),
                    .first_instance = 0
                });
                instances += each.instances;
            }
            _object_offset_storage[_frame_index]->push_back(offset);
            offset += meshes.size();
            index++;
        }
        _object_storage[_frame_index]->insert(object_data);
        _indirect_commands->resize(object_data.size());
        _object_remap_storage->resize(object_data.size());
        _instance_remap_storage->resize(instances);
        _draw_count_storage->resize(_scene.meshes.size());
    }

    void _make_depth_pyramid(am::uint32 width, am::uint32 height) noexcept {
        AM_PROFILE_SCOPED();
        width = previous_power_2(width);
        height = previous_power_2(height);
        _device->cleanup_after(
            am::frames_in_flight + 1,
            [i0 = std::move(_depth_pyramid),
             i1 = std::move(_depth_pyramid_views)](const am::CDevice*) mutable noexcept {});
        _depth_pyramid = am::CImage::make(_device, {
            .usage = am::EImageUsage::Storage |
                     am::EImageUsage::Sampled,
            .format = { am::EResourceFormat::R32Sfloat },
            .mips = compute_mips(width, height),
            .width = width,
            .height = height
        });
        _depth_pyramid_views.resize(_depth_pyramid->mips());
        for (am::uint32 index = 0; auto& view : _depth_pyramid_views) {
            view = am::CImageView::make(_device, {
                .image = _depth_pyramid.as_const(),
                .mip = index++
            });
        }
        _device->graphics_queue()->immediate_submit([this](am::CCommandBuffer& commands) noexcept {
            commands.transition_layout({
                .image = _depth_pyramid.get(),
                .source_stage = am::EPipelineStage::TopOfPipe,
                .dest_stage = am::EPipelineStage::ComputeShader,
                .source_access = am::EResourceAccess::None,
                .dest_access = am::EResourceAccess::ShaderWrite,
                .old_layout = am::EImageLayout::Undefined,
                .new_layout = am::EImageLayout::General
            });
        });
        _depth_pyramid_data.resize(_depth_pyramid->mips() + 1);
        for (am::uint32 i = 0; i < _depth_pyramid_data.size(); ++i) {
            if (i == 0) {
                _depth_pyramid_data[i].view = am::CImageView::from_image(_visibility_framebuffer->image(1));
            } else {
                _depth_pyramid_data[i].view = _depth_pyramid_views[i - 1];
            }
            if (!_depth_pyramid_data[i].set) {
                _depth_pyramid_data[i].set = am::CDescriptorSet::make(_device, {
                    .pool = _descriptor_pool,
                    .pipeline = _depth_reduce_pipeline,
                    .index = 0
                });
            }
        }
    }

    void _draw_ui(am::CCommandBuffer& commands) noexcept {
        AM_PROFILE_SCOPED();
        _ui_context->render_frame(commands, [this]() noexcept {
            AM_PROFILE_SCOPED();
            // ImGui::ShowDemoWindow();
            // ImPlot::ShowDemoWindow();
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
            {
                const auto flags =
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                    ImGuiWindowFlags_NoNavFocus;
                ImGui::Begin("main_dockspace", nullptr, flags);
            }
            ImGui::PopStyleVar(3);
            ImGui::DockSpace(ImGui::GetID("main_dockspace"));
            ImGui::End();

            ImGui::SetNextWindowSizeConstraints({ 64, 64 }, { 8192, 8192 });
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
            ImGui::Begin("viewport");
            ImGui::PopStyleVar(3);
            if (!ImGui::IsAnyMouseDown()) {
                _viewport_size = ImGui::GetContentRegionAvail();
            }
            _ui_set->bind(
                am::make_descriptor_binding(0, am::EDescriptorType::CombinedImageSampler),
                _device->sample(_final_framebuffer->image(0), {
                    .filter = am::EFilter::Nearest,
                    .mip_mode = am::EMipMode::Nearest,
                    .border_color = am::EBorderColor::FloatOpaqueBlack,
                    .address_mode = am::EAddressMode::Repeat
                }));
            ImGui::Image((ImTextureID)_ui_set->native(), _viewport_size);
            ImGui::End();

            ImGui::Begin("settings");
            ImGui::Text("device: %s", _device->properties().deviceName);
            ImGui::Text("fps: %.4f - %.4fms", 1 / _delta_time, _delta_time * 1000);
            ImGui::Text("allocator info:");
            {
                for (am::uint32 index = 0; const auto& heap : _device->heap_budgets()) {
                    char label[32];
                    std::snprintf(label, 32, "memory heap: %d", index++);
                    if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Text(" - block count: %d", heap.block_count);
                        ImGui::Text(" - allocation count: %d", heap.allocation_count);
                        ImGui::Text(" - block size: %llukB", heap.block_bytes / 1024);
                        ImGui::Text(" - allocation size: %llukB", heap.allocation_bytes / 1024);
                    }
                }
                ImGui::Separator();
            }
            {
                if (ImGui::CollapsingHeader("frame time plot", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (ImPlot::BeginPlot("frame time", { -1, 0 }, ImPlotFlags_Crosshairs)) {
                        const auto axes_flags =
                            ImPlotAxisFlags_AutoFit |
                            ImPlotAxisFlags_RangeFit |
                            ImPlotAxisFlags_NoInitialFit;
                        ImPlot::SetupAxes(nullptr, "ms", axes_flags | ImPlotAxisFlags_NoTickLabels, axes_flags);
                        auto time = std::vector<am::float64>(512);
                        std::iota(time.begin(), time.end(), 0);
                        auto history = std::vector(_state.delta_time_history.begin(), _state.delta_time_history.end());
                        ImPlot::PlotLine("delta_time", time.data(), history.data(), 512);
                        ImPlot::EndPlot();
                    }
                }
                ImGui::Separator();
            }
            {
                constexpr const char* names[] = {
                    "input_assembly_vertices",
                    "input_assembly_primitives",
                    "vertex_shader_invocations",
                    "geometry_shader_invocations",
                    "geometry_shader_primitives",
                    "clipping_invocations",
                    "clipping_primitives",
                    "fragment_shader_invocations",
                    "compute_shader_invocations"
                };
                if (ImGui::CollapsingHeader("pipeline statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (am::uint32 i = 0; i < _state.pipeline_statistics.size(); ++i) {
                        ImGui::Text(" - %s: %llu", names[i], _state.pipeline_statistics[i]);
                    }
                }
                ImGui::Separator();
            }
            if (ImGui::Checkbox("vsync", &_state.vsync)) {
                _swapchain->set_lost();
            }
            ImGui::Checkbox("frustum culling", &_state.frustum_culling);
            ImGui::Checkbox("occlusion culling", &_state.occlusion_culling);
            ImGui::End();

            ImGui::Begin("scene");
            if (ImGui::CollapsingHeader("point lights", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Button("add")) {
                    _state.point_lights.push_back({
                        .position = glm::vec3(0),
                        .diffuse = glm::vec3(1.0f),
                        .specular = glm::vec3(1.0f),
                        .radius = 5.0f
                    });
                }
                for (am::uint32 index = 0; auto& each : _state.point_lights) {
                    char label[64];
                    std::snprintf(label, 64, "point light: %d", index++);
                    if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::PushID(label);
                        const auto input_flags =
                            ImGuiInputTextFlags_AutoSelectAll |
                            ImGuiInputTextFlags_AlwaysInsertMode |
                            ImGuiInputTextFlags_AlwaysOverwrite;
                        ImGui::InputFloat3("position", &each.position.x, "%.3f", input_flags);
                        ImGui::InputFloat3("diffuse color", &each.diffuse.x, "%.3f", input_flags);
                        ImGui::InputFloat3("specular color", &each.specular.x, "%.3f", input_flags);
                        ImGui::DragFloat("radius", &each.radius);
                        ImGui::PopID();
                    }
                }
                ImGui::Separator();
            }
            if (ImGui::CollapsingHeader("directional lights", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::CollapsingHeader("sun light", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::PushID("sun light");
                    const auto input_flags =
                        ImGuiInputTextFlags_AutoSelectAll |
                        ImGuiInputTextFlags_AlwaysInsertMode |
                        ImGuiInputTextFlags_AlwaysOverwrite;
                    ImGui::InputFloat3("direction", &_state.directional_light_position.x, "%.3f", input_flags);
                    ImGui::PopID();
                }
                ImGui::Separator();
            }
            if (ImGui::CollapsingHeader("transforms", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (am::uint32 m_idx = 0; auto& draw : _draws) {
                    char m_label[32];
                    std::snprintf(m_label, 32, "model %d", m_idx);
                    if (ImGui::CollapsingHeader(m_label, ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::PushID(m_label);
                        for (am::uint32 t_idx = 0; auto& transform : draw.transform) {
                            char t_label[32];
                            std::snprintf(t_label, 32, "instance %d", t_idx);
                            if (ImGui::CollapsingHeader(t_label)) {
                                ImGui::PushID(t_label);
                                ImGui::InputFloat3("position", &transform.position.x);
                                ImGui::InputFloat3("rotation axis", &transform.rotation.axis.x);
                                ImGui::InputFloat("rotation angle", &transform.rotation.angle);
                                ImGui::InputFloat3("scale", &transform.scale.x);
                                ImGui::PopID();
                            }
                            t_idx++;
                        }
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                    m_idx++;
                }
                ImGui::Separator();
            }
            ImGui::End();

            ImGui::BeginMainMenuBar();
            if (ImGui::BeginMenu("file")) {
                if (ImGui::MenuItem("exit")) {
                    _window->close();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();

        });
    }

    void _store_pipeline_statistics() noexcept {
        AM_PROFILE_SCOPED();
        _state.pipeline_statistics = _pipeline_statistics->results();
    }

    // Main systems
    std::unique_ptr<am::CWindowingSystem> _system;
    am::CRcPtr<am::CWindow> _window;
    std::unique_ptr<am::CInput> _input;
    am::CRcPtr<am::CContext> _context;
    am::CRcPtr<am::CDevice> _device;
    am::CRcPtr<am::CSwapchain> _swapchain;
    am::CRcPtr<am::CDescriptorPool> _descriptor_pool;

    // Various graphics stuff
    am::CRcPtr<am::CRenderPass> _shadow_pass;
    am::CRcPtr<am::CRenderPass> _visibility_pass;
    am::CRcPtr<am::CRenderPass> _final_pass;
    am::CRcPtr<am::CFramebuffer> _shadow_framebuffer;
    am::CRcPtr<am::CFramebuffer> _visibility_framebuffer;
    am::CRcPtr<am::CFramebuffer> _final_framebuffer;
    am::CRcPtr<am::CPipeline> _shadow_pipeline;
    am::CRcPtr<am::CPipeline> _depth_reduce_pipeline;
    am::CRcPtr<am::CPipeline> _cull_pipeline;
    am::CRcPtr<am::CPipeline> _visibility_pipeline;
    am::CRcPtr<am::CPipeline> _final_pipeline;
    am::CRcPtr<am::CQueryPool> _pipeline_statistics;
    std::unique_ptr<am::CUIContext> _ui_context;
    std::vector<am::CRcPtr<am::CCommandBuffer>> _commands;

    // Descriptors
    std::vector<am::CRcPtr<am::CDescriptorSet>> _shadow_set;
    std::vector<am::CRcPtr<am::CDescriptorSet>> _cull_set;
    std::vector<am::CRcPtr<am::CDescriptorSet>> _visibility_set;
    std::vector<am::CRcPtr<am::CDescriptorSet>> _final_set;
    std::vector<am::CRcPtr<am::CDescriptorSet>> _light_set;
    am::CRcPtr<am::CDescriptorSet> _ui_set;
    ImVec2 _viewport_size = {};

    // Data
    am::CRcPtr<am::CImage> _depth_pyramid;
    std::vector<SDepthPyramidData> _depth_pyramid_data;
    std::vector<am::CRcPtr<am::CImageView>> _depth_pyramid_views;
    std::vector<am::CRcPtr<am::CTypedBuffer<SShadowCascade>>> _shadow_cascade_uniform;
    std::vector<am::CRcPtr<am::CTypedBuffer<SCameraData>>> _camera_uniform;
    std::vector<am::CRcPtr<am::CTypedBuffer<STransformData>>> _local_transform_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<STransformData>>> _world_transform_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<SPointLight>>> _point_light_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<SDirectionalLight>>> _directional_light_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<SObjectData>>> _object_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<am::SDrawCommandIndexedIndirect>>> _shadow_indirect_commands;
    am::CRcPtr<am::CTypedBuffer<am::SDrawCommandIndexedIndirect>> _indirect_commands;
    std::vector<am::CRcPtr<am::CTypedBuffer<am::uint32>>> _object_offset_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<am::uint32>>> _instance_offset_storage;
    am::CRcPtr<am::CTypedBuffer<am::uint32>> _draw_count_storage;
    am::CRcPtr<am::CTypedBuffer<am::uint32>> _object_remap_storage;
    am::CRcPtr<am::CTypedBuffer<am::uint32>> _instance_remap_storage;

    // Scene data
    am::tst::SScene _scene;
    am::tst::CCamera _camera;
    SCameraData _old_camera;
    std::vector<am::tst::SDraw> _draws;
    am::CRcPtr<am::CAsyncTexture> _default_texture;
    std::vector<am::CRcPtr<am::CAsyncModel>> _models;

    // Synchronization
    std::vector<am::CRcPtr<am::CFence>> _fences;
    std::vector<am::CRcPtr<am::CSemaphore>> _image_acq;
    std::vector<am::CRcPtr<am::CSemaphore>> _graphics_done;

    // Frame counting
    am::uint32 _image_index = 0;
    am::uint32 _frame_index = 0;
    am::uint64 _frame_count = 0;
    am::float64 _last_time = 0;
    am::float64 _delta_time = 0;
    am::float64 _frames = 0;

    // State config
    SEngineState _state = {};
    bool _occlusion_cull = false;
};

int main() {
    Application application;
    application.run();
    return 0;
}
