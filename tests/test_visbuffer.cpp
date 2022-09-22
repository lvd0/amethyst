#include <amethyst/graphics/descriptor_pool.hpp>
#include <amethyst/graphics/command_buffer.hpp>
#include <amethyst/graphics/descriptor_set.hpp>
#include <amethyst/graphics/typed_buffer.hpp>
#include <amethyst/graphics/render_pass.hpp>
#include <amethyst/graphics/async_model.hpp>
#include <amethyst/graphics/framebuffer.hpp>
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

#include "visbuffer/common.glsl"
#include "common.hpp"

namespace am::tst {
    struct SSubMesh {
        const STexturedMesh* mesh;
        uint32 textures[3];
        uint32 transform[2];
        uint32 instances;
    };

    struct SScene {
        using MeshBuffer = std::pair<const CTypedBuffer<uint8>*, const CTypedBuffer<uint8>*>;
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
            model->wait();
            uint32 old_world_offset = 0;
            uint32 old_instance_size = 0;
            for (uint32 j = 0; const auto& submesh : model->submeshes()) {
                submesh.geometry->wait();
                const auto mesh_buffer = SScene::MeshBuffer {
                    submesh.geometry->vertices()->handle,
                    submesh.geometry->indices()->handle
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
                scene.local_transforms.push_back({ submesh.transform, prev_transform });
                j++;
            }
            for (uint32 index = 0; const auto& transform : transforms) {
                auto result = glm::scale(glm::mat4(1.0f), transform.scale);
                AM_LIKELY_IF(std::fpclassify(transform.rotation.angle) != FP_ZERO) {
                    result = glm::rotate(result, transform.rotation.angle, transform.rotation.axis);
                }
                result = glm::translate(result, transform.position);
                auto prev_transform = glm::mat4(1.0f);
                AM_LIKELY_IF(index < old_instance_size) {
                    prev_transform = old_scene.world_transforms[old_world_offset + index].current;
                }
                scene.world_transforms.push_back({ result, prev_transform });
                index++;
            }
            world_offset += (uint32)transforms.size();
            i++;
        }
        return scene;
    }
} // namespace am::tst

static inline am::float32 compute_halton(am::uint32 i, am::uint32 b) {
    auto f = 1.0f;
    auto r = 0.0f;

    while (i > 0) {
        f /= (am::float32)b;
        r = r + f * (am::float32)(i % b);
        i = (am::uint32)std::floor((am::float32)i / (am::float32)b);
    }

    return r;
}

static inline am::CPipeline::SGraphicsCreateInfo visibility_pipeline_info(const am::CFramebuffer* framebuffer) noexcept {
    return {
        .vertex = "../data/shaders/visbuffer/visibility.vert",
        .fragment = "../data/shaders/visbuffer/visibility.frag",
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
    return {
        .vertex = "../data/shaders/visbuffer/final.vert",
        .fragment = "../data/shaders/visbuffer/final.frag",
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

static inline am::CPipeline::SGraphicsCreateInfo taa_resolve_pipeline_info(const am::CFramebuffer* framebuffer) noexcept {
    return {
        .vertex = "../data/shaders/visbuffer/taa_resolve.vert",
        .fragment = "../data/shaders/visbuffer/taa_resolve.frag",
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
        _swapchain = am::CSwapchain::make(_device, _window);
        _descriptor_pool = am::CDescriptorPool::make(_device);
        _visibility_pass = am::CRenderPass::make(_device, {
            .attachments = { {
                .format = am::EResourceFormat::R32G32Uint,
                .samples = am::EImageSampleCount::s1,
                .layout = {
                    .initial = am::EImageLayout::Undefined,
                    .final = am::EImageLayout::General
                },
                .clear = am::CClearValue::make({ .u32 = {} })
            }, {
                .format = am::EResourceFormat::R16G16Sfloat,
                .samples = am::EImageSampleCount::s1,
                .layout = {
                    .initial = am::EImageLayout::Undefined,
                    .final = am::EImageLayout::ShaderReadOnlyOptimal
                },
                .clear = am::CClearValue::make({ .f32 = {} })
            }, {
                .format = am::EResourceFormat::D32Sfloat,
                .samples = am::EImageSampleCount::s1,
                .layout = {
                    .initial = am::EImageLayout::Undefined,
                    .final = am::EImageLayout::ShaderReadOnlyOptimal
                },
                .clear = am::CClearValue::make({ .depth = 1 }),
            } },
            .subpasses = { {
                .attachments = { 0, 1, 2 },
                .preserve = {},
                .input = {}
            } },
            .dependencies = { {
                .source_subpass = am::external_subpass,
                .dest_subpass = 0,
                .source_stage = am::EPipelineStage::ColorAttachmentOutput |
                                am::EPipelineStage::EarlyFragmentTests,
                .dest_stage = am::EPipelineStage::ColorAttachmentOutput |
                              am::EPipelineStage::EarlyFragmentTests,
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
                .dest_stage = am::EPipelineStage::ColorAttachmentOutput |
                              am::EPipelineStage::EarlyFragmentTests |
                              am::EPipelineStage::LateFragmentTests |
                              am::EPipelineStage::FragmentShader,
                .source_access = am::EResourceAccess::ColorAttachmentWrite |
                                 am::EResourceAccess::DepthStencilAttachmentWrite,
                .dest_access = am::EResourceAccess::ColorAttachmentWrite |
                               am::EResourceAccess::DepthStencilAttachmentWrite |
                               am::EResourceAccess::ShaderRead
            } }
        });
        _final_pass = am::CRenderPass::make(_device, {
            .attachments = { {
                .format = _swapchain->format(),
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
                .source_stage = am::EPipelineStage::ColorAttachmentOutput |
                                am::EPipelineStage::FragmentShader,
                .dest_stage = am::EPipelineStage::ColorAttachmentOutput |
                              am::EPipelineStage::FragmentShader,
                .source_access = am::EResourceAccess::ColorAttachmentWrite |
                                 am::EResourceAccess::ShaderWrite,
                .dest_access = am::EResourceAccess::ColorAttachmentWrite |
                               am::EResourceAccess::ShaderRead
            }, {
                .source_subpass = 0,
                .dest_subpass = am::external_subpass,
                .source_stage = am::EPipelineStage::ColorAttachmentOutput,
                .dest_stage = am::EPipelineStage::FragmentShader,
                .source_access = am::EResourceAccess::ColorAttachmentWrite,
                .dest_access = am::EResourceAccess::ShaderRead
            } }
        });
        _taa_resolve_pass = am::CRenderPass::make(_device, {
            .attachments = { {
                .format = _swapchain->format(),
                .samples = am::EImageSampleCount::s1,
                .layout = {
                    .initial = am::EImageLayout::Undefined,
                    .final = am::EImageLayout::TransferSRCOptimal
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
                .dest_stage = am::EPipelineStage::Transfer,
                .source_access = am::EResourceAccess::ColorAttachmentWrite,
                .dest_access = am::EResourceAccess::TransferRead
            } }
        });
        _visibility_framebuffer = am::CFramebuffer::make(_device, {
            .width = _swapchain->width(),
            .height = _swapchain->height(),
            .attachments = { {
                am::make_attachment({
                    .index = 0,
                    .usage = am::EImageUsage::ColorAttachment |
                             am::EImageUsage::Storage,
                    .layers = 1,
                    .mips = 1
                }),
                am::make_attachment({
                    .index = 1,
                    .usage = am::EImageUsage::ColorAttachment |
                             am::EImageUsage::Sampled,
                    .layers = 1,
                    .mips = 1
                }),
                am::make_attachment({
                    .index = 2,
                    .usage = am::EImageUsage::DepthStencilAttachment |
                             am::EImageUsage::Sampled,
                    .layers = 1,
                    .mips = 1
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
                    .layers = 1,
                    .mips = 1
                })
            } },
            .pass = _final_pass
        });
        _taa_resolve_framebuffer = am::CFramebuffer::make(_device, {
            .width = _swapchain->width(),
            .height = _swapchain->height(),
            .attachments = { {
                am::make_attachment({
                    .index = 0,
                    .usage = am::EImageUsage::ColorAttachment |
                             am::EImageUsage::TransferSRC,
                    .layers = 1,
                    .mips = 1
                })
            } },
            .pass = _taa_resolve_pass
        });
        _visibility_pipeline = am::CPipeline::make(_device, visibility_pipeline_info(_visibility_framebuffer.get()));
        _final_pipeline = am::CPipeline::make(_device, final_pipeline_info(_final_framebuffer.get()));
        _taa_resolve_pipeline = am::CPipeline::make(_device, taa_resolve_pipeline_info(_taa_resolve_framebuffer.get()));
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
            am::CAsyncModel::make(_device, "../data/models/cube/Cube.gltf"),
        };

        _draws = { {
            .model = _models[0].get(),
            .transform = { {
                .position = {},
                .rotation = {},
                .scale = glm::vec3(1.0f)
            } }
        } };

        _default_texture = am::CAsyncTexture::sync_make(_device, {
            .path = "../data/textures/default.ktx2",
            .type = am::ETextureType::Color
        });

        _make_taa_resources();

        _visibility_set = am::CDescriptorSet::make(_device, am::frames_in_flight, {
            .pool = _descriptor_pool,
            .pipeline = _visibility_pipeline,
            .layout = 0
        });
        _final_set = am::CDescriptorSet::make(_device, am::frames_in_flight, {
            .pool = _descriptor_pool,
            .pipeline = _final_pipeline,
            .layout = 0
        });
        _light_set = am::CDescriptorSet::make(_device, am::frames_in_flight, {
            .pool = _descriptor_pool,
            .pipeline = _final_pipeline,
            .layout = 1
        });
        _taa_resolve_set = am::CDescriptorSet::make(_device, am::frames_in_flight, {
            .pool = _descriptor_pool,
            .pipeline = _taa_resolve_pipeline,
            .layout = 0
        });
        _camera_uniform = am::CTypedBuffer<SCameraData>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::UniformBuffer,
            .memory = am::memory_auto,
            .capacity = 1,
        });
        _local_transform_storage = am::CTypedBuffer<STransformData>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 64,
        });
        _world_transform_storage = am::CTypedBuffer<STransformData>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 64,
        });
        _point_light_storage = am::CTypedBuffer<SPointLight>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 64,
        });
        _directional_light_storage = am::CTypedBuffer<SDirectionalLight>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 64,
        });
        _object_storage = am::CTypedBuffer<SObjectData>::make(_device, am::frames_in_flight, {
            .usage = am::EBufferUsage::StorageBuffer,
            .memory = am::memory_auto,
            .capacity = 64,
        });
        _indirect_commands = {
            am::CTypedBuffer<am::SDrawCommandIndexedIndirect>::make(_device, am::frames_in_flight, {
                .usage = am::EBufferUsage::IndirectBuffer,
                .memory = am::memory_auto,
                .capacity = 1024,
            })
        };
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
        if (_frames >= 2) {
            spdlog::info("dt: {}ms, fps: {}", _delta_time * 1000, 1 / _delta_time);
            _frames = 0;
        }

        _system->poll_events();
        while (_window->width() == 0 || _window->height() == 0) {
            _system->wait_events();
            _window->update_viewport();
        }
        _input->capture();
        _camera.update((am::float32)_delta_time);
        _scene = build_scene(_device.get(), _draws, _default_texture.get(), _scene);
        _fences[_frame_index]->wait_and_reset();
        _build_indirect_commands();

        if (_input->is_key_pressed_once(am::Keyboard::kR)) {
            _device->cleanup_after(
                am::frames_in_flight + 1,
                [p0 = std::move(_visibility_pipeline),
                 p1 = std::move(_final_pipeline)](const am::CDevice*) mutable noexcept {});
            _visibility_pipeline = am::CPipeline::make(_device, visibility_pipeline_info(_visibility_framebuffer.get()));
            _final_pipeline = am::CPipeline::make(_device, final_pipeline_info(_final_framebuffer.get()));
            _frame_count = 0;
        }

        SCameraData buffer[2] = {};
        buffer[0].projection = _camera.projection();
        buffer[0].view = _camera.view();
        buffer[0].proj_view = buffer[0].projection * buffer[0].view;
        buffer[0].position = _camera.position();
        buffer[1] = _old_camera;
        _old_camera = buffer[0];

        _camera_uniform[_frame_index]->insert(buffer, sizeof buffer);

        _local_transform_storage[_frame_index]->insert(_scene.local_transforms);
        _world_transform_storage[_frame_index]->insert(_scene.world_transforms);

        _point_light_storage[_frame_index]->insert({ {
            .position = glm::vec3(5.0f, 3.0f, -0.5f),
            .diffuse = glm::vec3(5.0f),
            .specular = glm::vec3(1.0f),
            .radius = 50.0f
        }, {
            .position = glm::vec3(-5.0f, 3.0f, -0.5f),
            .diffuse = glm::vec3(5.0f),
            .specular = glm::vec3(1.0f),
            .radius = 50.0f
        } });
        _directional_light_storage[_frame_index]->resize(8);

        _image_index = _device->acquire_image(_swapchain.get(), _image_acq[_frame_index].get());
        AM_UNLIKELY_IF(_swapchain->is_lost()) {
            _resize_resources();
            _frame_count = 0;
        }

        _visibility_set[_frame_index]->bind("UCamera", _camera_uniform[_frame_index]->info());
        _visibility_set[_frame_index]->bind("BLocalTransforms", _local_transform_storage[_frame_index]->info());
        _visibility_set[_frame_index]->bind("BWorldTransforms", _world_transform_storage[_frame_index]->info());
        _visibility_set[_frame_index]->bind("BObjectData", _object_storage[_frame_index]->info());

        _final_set[_frame_index]->bind("UCamera", _camera_uniform[_frame_index]->info());
        _final_set[_frame_index]->bind("BObjectData", _object_storage[_frame_index]->info());
        _final_set[_frame_index]->bind("BLocalTransforms", _local_transform_storage[_frame_index]->info());
        _final_set[_frame_index]->bind("BWorldTransforms", _world_transform_storage[_frame_index]->info());
        _final_set[_frame_index]->bind("u_visibility", _visibility_framebuffer->image(0));
        _final_set[_frame_index]->bind("u_textures", _scene.textures);

        _light_set[_frame_index]->bind("BPointLights", _point_light_storage[_frame_index]->info());
        _light_set[_frame_index]->bind("BDirectionalLights", _directional_light_storage[_frame_index]->info());

        const auto history = _device->sample(_taa_history.get(), {
            .filter = am::EFilter::Linear,
            .border_color = am::EBorderColor::IntOpaqueBlack,
            .address_mode = am::EAddressMode::ClampToBorder
        });
        const auto velocity = _device->sample(_visibility_framebuffer->image(1), {
            .filter = am::EFilter::Nearest,
            .border_color = am::EBorderColor::FloatOpaqueBlack,
            .address_mode = am::EAddressMode::ClampToBorder
        });
        const auto final_color = _device->sample(_final_framebuffer->image(0), {
            .filter = am::EFilter::Linear,
            .border_color = am::EBorderColor::IntOpaqueBlack,
            .address_mode = am::EAddressMode::ClampToBorder
        });
        const auto scene_depth = _device->sample(_visibility_framebuffer->image(2), {
            .filter = am::EFilter::Linear,
            .border_color = am::EBorderColor::FloatOpaqueBlack,
            .address_mode = am::EAddressMode::ClampToBorder
        });
        _taa_resolve_set[_frame_index]->bind("u_taa_history", history);
        _taa_resolve_set[_frame_index]->bind("u_taa_velocity", velocity);
        _taa_resolve_set[_frame_index]->bind("u_final_color", final_color);
        _taa_resolve_set[_frame_index]->bind("u_scene_depth", scene_depth);
    }

    void render() noexcept {
        AM_PROFILE_SCOPED();
        auto& commands = *_commands[_frame_index];
        commands
            .begin()
            .begin_render_pass(_visibility_framebuffer.get())
            .bind_pipeline(_visibility_pipeline.get())
            .bind_descriptor_set(_visibility_set[_frame_index].get())
            .set_viewport(am::inverted_viewport_tag)
            .set_scissor();
        {
            am::uint32 object_offset = 0;
            for (am::uint32 index = 0; const auto& [mesh_buffer, meshes] : _scene.meshes) {
                const auto& [vertex_buffer, index_buffer] = mesh_buffer;
#pragma pack(push, 1)
                struct Constants {
                    am::uint32 object_offset;
                    glm::vec2 jitter_offset;
                } constants = {
                    object_offset,
                    _generate_jitter(_frame_count % 16)
                };
#pragma pack(pop)
                commands
                    .bind_vertex_buffer(vertex_buffer->info())
                    .bind_index_buffer(index_buffer->info())
                    .push_constants(am::EShaderStage::Vertex, &constants, sizeof constants)
                    .draw_indexed_indirect(_indirect_commands[index++][_frame_index]->info());
                object_offset += meshes.size();
            }
        }

        const auto object_count = (am::uint32)_object_storage[_frame_index]->size();
        const auto discard_history = (am::uint32)(_frame_count == 0);
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
            .end_render_pass()
            .transition_layout({
                .image = _taa_history.get(),
                .source_stage = am::EPipelineStage::Transfer,
                .dest_stage = am::EPipelineStage::FragmentShader,
                .source_access = am::EResourceAccess::TransferWrite,
                .dest_access = am::EResourceAccess::ShaderRead,
                .old_layout = am::EImageLayout::TransferDSTOptimal,
                .new_layout = am::EImageLayout::ShaderReadOnlyOptimal
            })
            .begin_render_pass(_taa_resolve_framebuffer.get())
            .bind_pipeline(_taa_resolve_pipeline.get())
            .bind_descriptor_set(_taa_resolve_set[_frame_index].get())
            .set_viewport()
            .set_scissor()
            .push_constants(am::EShaderStage::Fragment, &discard_history, sizeof discard_history)
            .draw(3, 1, 0, 0)
            .end_render_pass()
            .transition_layout({
                .image = _taa_history.get(),
                .source_stage = am::EPipelineStage::FragmentShader,
                .dest_stage = am::EPipelineStage::Transfer,
                .source_access = am::EResourceAccess::None,
                .dest_access = am::EResourceAccess::TransferWrite,
                .old_layout = am::EImageLayout::ShaderReadOnlyOptimal,
                .new_layout = am::EImageLayout::TransferDSTOptimal
            })
            .copy_image(_taa_resolve_framebuffer->image(0), _taa_history.get())
            .transition_layout({
                .image = _swapchain->image(_image_index),
                .source_stage = am::EPipelineStage::TopOfPipe,
                .dest_stage = am::EPipelineStage::Transfer,
                .source_access = am::EResourceAccess::None,
                .dest_access = am::EResourceAccess::TransferWrite,
                .old_layout = am::EImageLayout::Undefined,
                .new_layout = am::EImageLayout::TransferDSTOptimal
            })
            .copy_image(_taa_resolve_framebuffer->image(0), _swapchain->image(_image_index))
            .transition_layout({
                .image = _swapchain->image(_image_index),
                .source_stage = am::EPipelineStage::Transfer,
                .dest_stage = am::EPipelineStage::BottomOfPipe,
                .source_access = am::EResourceAccess::TransferWrite,
                .dest_access = am::EResourceAccess::None,
                .old_layout = am::EImageLayout::TransferDSTOptimal,
                .new_layout = am::EImageLayout::PresentSRC
            })
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

        AM_UNLIKELY_IF(_swapchain->is_lost()) {
            _resize_resources();
            _frame_count = -1;
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
        _swapchain->recreate();
        _visibility_framebuffer->resize(_swapchain->width(), _swapchain->height());
        _final_framebuffer->resize(_swapchain->width(), _swapchain->height());
        _taa_resolve_framebuffer->resize(_swapchain->width(), _swapchain->height());
        _device->cleanup_after(
            am::frames_in_flight + 1,
            [d0 = std::move(_taa_history)](const am::CDevice*) mutable noexcept {});
        _make_taa_resources();
    }

    void _build_indirect_commands() noexcept {
        AM_PROFILE_SCOPED();
        AM_UNLIKELY_IF(_scene.meshes.size() > _indirect_commands.size()) {
            _indirect_commands.resize(_scene.meshes.size());
        }
        std::vector<SObjectData> object_data;
        object_data.reserve(1024);
        for (am::uint32 index = 0; const auto& [mesh_buffer, meshes] : _scene.meshes) {
            const auto& [vertex_buffer, index_buffer] = mesh_buffer;
            AM_UNLIKELY_IF(_indirect_commands[index].empty()) {
                _indirect_commands[index] = am::CTypedBuffer<am::SDrawCommandIndexedIndirect>::make(_device, am::frames_in_flight, {
                    .usage = am::EBufferUsage::IndirectBuffer,
                    .memory = am::memory_auto,
                    .capacity = 1024
                });
            }
            std::vector<am::SDrawCommandIndexedIndirect> indirect_buffer;
            indirect_buffer.reserve(1024);
            for (const auto& each : meshes) {
                const auto& geometry = each.mesh->geometry;
                indirect_buffer.push_back({
                    .indices = each.mesh->indices,
                    .instances = each.instances,
                    .first_index = (am::uint32)geometry->index_offset(),
                    .vertex_offset = (am::int32)geometry->vertex_offset(),
                    .first_instance = 0
                });
                object_data.push_back({
                    .transform_index = { each.transform[0], each.transform[1] },
                    .albedo_index = each.textures[0],
                    .normal_index = each.textures[1],
                    .specular_index = each.textures[2],
                    .vertex_address = vertex_buffer->address(),
                    .index_address = index_buffer->address(),
                    .vertex_offset = (am::int32)geometry->vertex_offset(),
                    .index_offset = (am::uint32)geometry->index_offset() / 3
                });
            }
            _indirect_commands[index++][_frame_index]->insert(indirect_buffer);
        }
        _object_storage[_frame_index]->insert(object_data);
    }

    void _make_taa_resources() noexcept {
        AM_PROFILE_SCOPED();
        _taa_history = am::CImage::make(_device, {
            .usage = am::EImageUsage::TransferDST | am::EImageUsage::Sampled,
            .format = _swapchain->format(),
            .width = _swapchain->width(),
            .height = _swapchain->height()
        });
        _device->graphics_queue()->immediate_submit([this](am::CCommandBuffer& commands) noexcept {
            commands.transition_layout({
                .image = _taa_history.get(),
                .source_stage = am::EPipelineStage::TopOfPipe,
                .dest_stage = am::EPipelineStage::Transfer,
                .source_access = am::EResourceAccess::None,
                .dest_access = am::EResourceAccess::TransferWrite,
                .old_layout = am::EImageLayout::Undefined,
                .new_layout = am::EImageLayout::TransferDSTOptimal
            });
        });
    }

    glm::vec2 _generate_jitter(am::uint32 index) noexcept {
        const auto halton_x = 2.0f * compute_halton(index + 1, 2) - 1.0f;
        const auto halton_y = 2.0f * compute_halton(index + 1, 3) - 1.0f;
        const auto jitter_x = (halton_x / (am::float32)_swapchain->width());
        const auto jitter_y = (halton_y / (am::float32)_swapchain->height());
        return { jitter_x, jitter_y };
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
    am::CRcPtr<am::CRenderPass> _visibility_pass;
    am::CRcPtr<am::CRenderPass> _final_pass;
    am::CRcPtr<am::CFramebuffer> _visibility_framebuffer;
    am::CRcPtr<am::CFramebuffer> _final_framebuffer;
    am::CRcPtr<am::CPipeline> _visibility_pipeline;
    am::CRcPtr<am::CPipeline> _final_pipeline;
    std::vector<am::CRcPtr<am::CCommandBuffer>> _commands;

    // TAA
    am::CRcPtr<am::CRenderPass> _taa_resolve_pass;
    am::CRcPtr<am::CFramebuffer> _taa_resolve_framebuffer;
    am::CRcPtr<am::CPipeline> _taa_resolve_pipeline;
    am::CRcPtr<am::CImage> _taa_history;
    std::vector<am::CRcPtr<am::CDescriptorSet>> _taa_resolve_set;

    // Descriptors
    std::vector<am::CRcPtr<am::CDescriptorSet>> _visibility_set;
    std::vector<am::CRcPtr<am::CDescriptorSet>> _final_set;
    std::vector<am::CRcPtr<am::CDescriptorSet>> _light_set;
    std::vector<am::CRcPtr<am::CTypedBuffer<SCameraData>>> _camera_uniform;
    std::vector<am::CRcPtr<am::CTypedBuffer<STransformData>>> _local_transform_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<STransformData>>> _world_transform_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<SPointLight>>> _point_light_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<SDirectionalLight>>> _directional_light_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<SObjectData>>> _object_storage;
    std::vector<std::vector<am::CRcPtr<am::CTypedBuffer<am::SDrawCommandIndexedIndirect>>>> _indirect_commands;
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
};

int main() {
    Application application;
    application.run();
    return 0;
}
