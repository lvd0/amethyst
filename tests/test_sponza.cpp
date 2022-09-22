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

#include "main/common.glsl"
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

    static inline SScene build_scene(CDevice* device, const std::vector<SDraw>& draws, CAsyncTexture* fallback) noexcept {
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
            for (uint32 j = 0; const auto& submesh : model->submeshes()) {
                submesh.geometry->wait();
                const auto mesh_buffer = SScene::MeshBuffer{ submesh.geometry->vertices()->handle, submesh.geometry->indices()->handle };
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
                current.push_back({
                    .mesh = &submesh,
                    .textures = { indices[0], indices[1] },
                    .transform = { local_offset++, world_offset },
                    .instances = (uint32)transforms.size(),
                });
                scene.local_transforms.push_back({ submesh.transform });
                j++;
            }
            for (const auto& transform : transforms) {
                auto result = glm::scale(glm::mat4(1.0f), transform.scale);
                AM_LIKELY_IF(std::fpclassify(transform.rotation.angle) != FP_ZERO) {
                    result = glm::rotate(result, transform.rotation.angle, transform.rotation.axis);
                }
                result = glm::translate(result, transform.position);
                scene.world_transforms.push_back({ result });
            }
            world_offset += (uint32)transforms.size();
            i++;
        }
        return scene;
    }
} // namespace am::tst

static inline am::CPipeline::SGraphicsCreateInfo depth_pipeline_info(const am::CFramebuffer* framebuffer) {
    return {
        .vertex = "../data/shaders/main/z_prepass.vert",
        .fragment = {},
        .geometry = {},
        .attributes = {
            am::EVertexAttribute::Vec3,
            am::EVertexAttribute::Vec3,
            am::EVertexAttribute::Vec2,
            am::EVertexAttribute::Vec4,
        },
        .attachments = {},
        .states = {
            am::EDynamicState::Viewport,
            am::EDynamicState::Scissor,
        },
        .cull = am::ECullMode::Back,
        .depth_test = true,
        .depth_write = true,
        .subpass = 0,
        .framebuffer = framebuffer
    };
}

static inline am::CPipeline::SGraphicsCreateInfo main_pipeline_info(const am::CFramebuffer* framebuffer) {
    return {
        .vertex = "../data/shaders/main/main.vert",
        .fragment = "../data/shaders/main/main.frag",
        .geometry = {},
        .attributes = {
            am::EVertexAttribute::Vec3,
            am::EVertexAttribute::Vec3,
            am::EVertexAttribute::Vec2,
            am::EVertexAttribute::Vec4,
        },
        .attachments = {},
        .states = {
            am::EDynamicState::Viewport,
            am::EDynamicState::Scissor,
        },
        .cull = am::ECullMode::Back,
        .depth_test = true,
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
        _depth_pass = am::CRenderPass::make(_device, {
            .attachments = { { // Depth Attachment
                .format = am::EResourceFormat::D32Sfloat,
                .samples = am::EImageSampleCount::s1,
                .layout = {
                    .initial = am::EImageLayout::Undefined,
                    .final = am::EImageLayout::DepthStencilAttachmentOptimal
                },
                .clear = am::CClearValue::make({ .depth = 1 }),
                .discard = false
            } },
            .subpasses = { {
                .attachments = { 0 },
                .preserve = {},
                .input = {}
            } },
            .dependencies = { {
                .source_subpass = am::external_subpass,
                .dest_subpass = 0,
                .source_stage = am::EPipelineStage::EarlyFragmentTests,
                .dest_stage = am::EPipelineStage::EarlyFragmentTests,
                .source_access = am::EResourceAccess::DepthStencilAttachmentWrite,
                .dest_access = am::EResourceAccess::DepthStencilAttachmentWrite
            } }
        });
        _main_pass = am::CRenderPass::make(_device, {
            .attachments = { { // Color Attachment
                .format = _swapchain->format(),
                .samples = am::EImageSampleCount::s1,
                .layout = {
                    .initial = am::EImageLayout::Undefined,
                    .final = am::EImageLayout::TransferSRCOptimal
                },
                .clear = am::CClearValue::make({ .f32 = { 0, 0, 0, 1 } }),
                .discard = false
            }, { // Depth Attachment
                .format = am::EResourceFormat::D32Sfloat,
                .samples = am::EImageSampleCount::s1,
                .layout = {
                    .initial = am::EImageLayout::DepthStencilAttachmentOptimal,
                    .final = am::EImageLayout::DepthStencilAttachmentOptimal
                },
                .clear = am::CClearValue::make(),
                .discard = false
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
                                am::EPipelineStage::EarlyFragmentTests,
                .dest_stage = am::EPipelineStage::ColorAttachmentOutput |
                              am::EPipelineStage::EarlyFragmentTests,
                .source_access = am::EResourceAccess::DepthStencilAttachmentWrite,
                .dest_access = am::EResourceAccess::ColorAttachmentWrite |
                               am::EResourceAccess::DepthStencilAttachmentRead
            }, {
                .source_subpass = 0,
                .dest_subpass = am::external_subpass,
                .source_stage = am::EPipelineStage::ColorAttachmentOutput,
                .dest_stage = am::EPipelineStage::Transfer,
                .source_access = am::EResourceAccess::ColorAttachmentWrite,
                .dest_access = am::EResourceAccess::TransferRead
            } }
        });
        _depth_framebuffer = am::CFramebuffer::make(_device, {
            .width = _swapchain->width(),
            .height = _swapchain->height(),
            .attachments = {
                am::make_attachment({ // Depth Attachment Framebuffer
                    .index = 0,
                    .usage = am::EImageUsage::DepthStencilAttachment,
                    .layers = 1,
                    .mips = 1
                })
            },
            .pass = _depth_pass
        });
        _color_framebuffer = am::CFramebuffer::make(_device, {
            .width = _swapchain->width(),
            .height = _swapchain->height(),
            .attachments = {
                am::make_attachment({
                    .index = 0,
                    .usage = am::EImageUsage::ColorAttachment | am::EImageUsage::TransferSRC,
                    .layers = 1,
                    .mips = 1
                }),
                am::make_attachment({
                    .index = 1,
                    .image = _depth_framebuffer->image(0)
                }),
            },
            .pass = _main_pass
        });
        _depth_pipeline = am::CPipeline::make(_device, depth_pipeline_info(_depth_framebuffer.get()));
        _main_pipeline = am::CPipeline::make(_device, main_pipeline_info(_color_framebuffer.get()));
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
            am::CAsyncModel::make(_device, "../data/models/sponza/Sponza.gltf"),
            am::CAsyncModel::make(_device, "../data/models/cube/Cube.gltf"),
            am::CAsyncModel::make(_device, "../data/models/deccer_cubes/SM_Deccer_Cubes_Textured.gltf"),
            am::CAsyncModel::make(_device, "../data/models/intel_sponza/intel_sponza.gltf"),
            am::CAsyncModel::make(_device, "../data/models/intel_sponza/curtains/curtains.gltf"),
        };

        _draws = { {
            .model = _models[0].get(),
            .transform = { {
                .position = {},
                .rotation = {},
                .scale = glm::vec3(1.0f)
            } }
        }, {
            .model = _models[3].get(),
            .transform = { {
                .position = glm::vec3(-30.0f, 0.0f, 0.0f),
                .rotation = {},
                .scale = glm::vec3(1.0f)
            } }
        }, {
            .model = _models[4].get(),
            .transform = { {
                .position = glm::vec3(-30.0f, 0.0f, 0.0f),
                .rotation = {},
                .scale = glm::vec3(1.0f)
            } }
        } };

        _default_texture = am::CAsyncTexture::sync_make(_device, {
            .path = "../data/textures/default.ktx2",
            .type = am::ETextureType::Color
        });

        _depth_set = am::CDescriptorSet::make(_device, am::frames_in_flight, {
            .pool = _descriptor_pool,
            .pipeline = _depth_pipeline,
            .layout = 0
        });
        _main_set = am::CDescriptorSet::make(_device, am::frames_in_flight, {
            .pool = _descriptor_pool,
            .pipeline = _main_pipeline,
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
        _object_data_storage = am::CTypedBuffer<SObjectData>::make(_device, am::frames_in_flight, {
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
        _frame_count++;
        const auto current_time = _system->current_time();
        _delta_time = current_time - _last_time;
        _last_time = current_time;
        _frames += _delta_time;
        if (_frames >= 2) {
            spdlog::info("dt: {}ms, fps: {}", (_frames / _frame_count) * 1000, 1 / (_frames / _frame_count));
            _frames = 0;
            _frame_count = 0;
        }

        _system->poll_events();
        while (_window->width() == 0 || _window->height() == 0) {
            _system->wait_events();
            _window->update_viewport();
        }
        _input->capture();
        _camera.update((am::float32)_delta_time);
        _scene = build_scene(_device.get(), _draws, _default_texture.get());
        _fences[_frame_index]->wait_and_reset();

        if (_input->is_key_pressed_once(am::Keyboard::kR)) {
            _device->cleanup_after(
                am::frames_in_flight,
                [p0 = std::move(_depth_pipeline),
                 p1 = std::move(_main_pipeline)](const am::CDevice*) mutable noexcept {});
            _depth_pipeline = am::CPipeline::make(_device, depth_pipeline_info(_depth_framebuffer.get()));
            _main_pipeline = am::CPipeline::make(_device, main_pipeline_info(_color_framebuffer.get()));
        }

        SCameraData buffer = {};
        buffer.projection = _camera.projection();
        buffer.view = _camera.view();
        buffer.proj_view = buffer.projection * buffer.view;
        _camera_uniform[_frame_index]->insert(buffer);
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
        _build_indirect_commands();

        _depth_set[_frame_index]->bind("UCamera", _camera_uniform[_frame_index]->info());
        _depth_set[_frame_index]->bind("BLocalTransforms", _local_transform_storage[_frame_index]->info());
        _depth_set[_frame_index]->bind("BWorldTransforms", _world_transform_storage[_frame_index]->info());
        _depth_set[_frame_index]->bind("BObjectData", _object_data_storage[_frame_index]->info());

        _main_set[_frame_index]->bind("UCamera", _camera_uniform[_frame_index]->info());
        _main_set[_frame_index]->bind("BLocalTransforms", _local_transform_storage[_frame_index]->info());
        _main_set[_frame_index]->bind("BWorldTransforms", _world_transform_storage[_frame_index]->info());
        _main_set[_frame_index]->bind("BObjectData", _object_data_storage[_frame_index]->info());
        _main_set[_frame_index]->bind("BPointLights", _point_light_storage[_frame_index]->info());
        _main_set[_frame_index]->bind("BDirectionalLights", _directional_light_storage[_frame_index]->info());
        _main_set[_frame_index]->bind("u_textures", _scene.textures);
    }

    void render() noexcept {
        AM_PROFILE_SCOPED();
        const auto image_index = _device->acquire_image(_swapchain.get(), _image_acq[_frame_index].get());
        AM_UNLIKELY_IF(_swapchain->is_lost()) {
            _resize_resources();
        }

        auto& command_buf = *_commands[_frame_index];
        command_buf
            .begin()
            .begin_render_pass(_depth_framebuffer.get())
            .bind_pipeline(_depth_pipeline.get())
            .bind_descriptor_set(_depth_set[_frame_index].get())
            .set_viewport(am::inverted_viewport_tag)
            .set_scissor();
        {
            am::uint32 object_offset = 0;
            for (am::uint32 index = 0; const auto& [mesh_buffer, meshes] : _scene.meshes) {
                const auto& [vertex_buffer, index_buffer] = mesh_buffer;
                command_buf
                    .bind_vertex_buffer(vertex_buffer->info())
                    .bind_index_buffer(index_buffer->info())
                    .push_constants(am::EShaderStage::Vertex, &object_offset, sizeof object_offset)
                    .draw_indexed_indirect(_indirect_commands[index++][_frame_index]->info());
                object_offset += meshes.size();
            }
        }

        command_buf
            .end_render_pass()
            .begin_render_pass(_color_framebuffer.get())
            .bind_pipeline(_main_pipeline.get())
            .bind_descriptor_set(_main_set[_frame_index].get())
            .set_viewport(am::inverted_viewport_tag)
            .set_scissor();
        {
            am::uint32 object_offset = 0;
            for (am::uint32 index = 0; const auto& [mesh_buffer, meshes] : _scene.meshes) {
                const auto& [vertex_buffer, index_buffer] = mesh_buffer;
                command_buf
                    .bind_vertex_buffer(vertex_buffer->info())
                    .bind_index_buffer(index_buffer->info())
                    .push_constants(am::EShaderStage::Vertex, &object_offset, sizeof object_offset)
                    .draw_indexed_indirect(_indirect_commands[index++][_frame_index]->info());
                object_offset += meshes.size();
            }
        }
        command_buf
            .end_render_pass()
            .transition_layout({
                .image = _swapchain->image(image_index),
                .source_stage = am::EPipelineStage::TopOfPipe,
                .dest_stage = am::EPipelineStage::Transfer,
                .source_access = am::EResourceAccess::None,
                .dest_access = am::EResourceAccess::TransferWrite,
                .old_layout = am::EImageLayout::Undefined,
                .new_layout = am::EImageLayout::TransferDSTOptimal,
                .layer = 0,
                .mip = 0
            })
            .copy_image(_color_framebuffer->image(0), _swapchain->image(image_index))
            .transition_layout({
                .image = _swapchain->image(image_index),
                .source_stage = am::EPipelineStage::Transfer,
                .dest_stage = am::EPipelineStage::BottomOfPipe,
                .source_access = am::EResourceAccess::TransferWrite,
                .dest_access = am::EResourceAccess::None,
                .old_layout = am::EImageLayout::TransferDSTOptimal,
                .new_layout = am::EImageLayout::PresentSRC,
                .layer = 0,
                .mip = 0
            })
            .end();

        _device->graphics_queue()->submit({ {
            .stage_mask = am::EPipelineStage::Transfer,
            .command = _commands[_frame_index].get(),
            .wait = _image_acq[_frame_index].get(),
            .signal = _graphics_done[_frame_index].get()
        } }, _fences[_frame_index].get());

        _device->graphics_queue()->present({
            .image = image_index,
            .swapchain = _swapchain.get(),
            .wait = _graphics_done[_frame_index].get()
        });

        AM_UNLIKELY_IF(_swapchain->is_lost()) {
            _resize_resources();
        }

        _device->update_cleanup();
        _frame_index = (_frame_index + 1) % am::frames_in_flight;
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
        _swapchain->recreate();
        _depth_framebuffer->resize(_swapchain->width(), _swapchain->height());
        _color_framebuffer->update_attachment(1, _depth_framebuffer->image(0));
        _color_framebuffer->resize(_swapchain->width(), _swapchain->height());
    }

    void _build_indirect_commands() noexcept {
        AM_UNLIKELY_IF(_scene.meshes.size() > _indirect_commands.size()) {
            _indirect_commands.resize(_scene.meshes.size());
        }
        auto& object_data = _object_data_storage[_frame_index];
        object_data->clear();
        for (am::uint32 index = 0; const auto& [mesh_buffer, meshes] : _scene.meshes) {
            AM_UNLIKELY_IF(_indirect_commands[index].empty()) {
                _indirect_commands[index] = am::CTypedBuffer<am::SDrawCommandIndexedIndirect>::make(_device, am::frames_in_flight, {
                    .usage = am::EBufferUsage::IndirectBuffer,
                    .memory = am::memory_auto,
                    .capacity = 1024
                });
            }
            auto& indirect_buffer = _indirect_commands[index][_frame_index];
            indirect_buffer->clear();
            for (const auto& each : meshes) {
                const auto& geometry = each.mesh->geometry;
                indirect_buffer->push_back({
                    .indices = each.mesh->indices,
                    .instances = each.instances,
                    .first_index = (am::uint32)geometry->index_offset(),
                    .vertex_offset = (am::int32)geometry->vertex_offset(),
                    .first_instance = 0
                });
                object_data->push_back({
                    .transform_index = { each.transform[0], each.transform[1] },
                    .albedo_index = each.textures[0],
                    .normal_index = each.textures[1],
                    .specular_index = each.textures[2]
                });
            }
            index++;
        }
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
    am::CRcPtr<am::CRenderPass> _main_pass;
    am::CRcPtr<am::CRenderPass> _depth_pass;
    am::CRcPtr<am::CFramebuffer> _color_framebuffer;
    am::CRcPtr<am::CFramebuffer> _depth_framebuffer;
    am::CRcPtr<am::CPipeline> _depth_pipeline;
    am::CRcPtr<am::CPipeline> _main_pipeline;
    std::vector<am::CRcPtr<am::CCommandBuffer>> _commands;

    // Models
    am::tst::SScene _scene;
    am::tst::CCamera _camera;
    std::vector<am::tst::SDraw> _draws;
    am::CRcPtr<am::CAsyncTexture> _default_texture;
    std::vector<am::CRcPtr<am::CAsyncModel>> _models;
    std::vector<am::CRcPtr<am::CDescriptorSet>> _main_set;
    std::vector<am::CRcPtr<am::CDescriptorSet>> _depth_set;
    std::vector<am::CRcPtr<am::CTypedBuffer<SCameraData>>> _camera_uniform;
    std::vector<am::CRcPtr<am::CTypedBuffer<STransformData>>> _local_transform_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<STransformData>>> _world_transform_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<SPointLight>>> _point_light_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<SDirectionalLight>>> _directional_light_storage;
    std::vector<am::CRcPtr<am::CTypedBuffer<SObjectData>>> _object_data_storage;
    std::vector<std::vector<am::CRcPtr<am::CTypedBuffer<am::SDrawCommandIndexedIndirect>>>> _indirect_commands;

    // Synchronization
    std::vector<am::CRcPtr<am::CFence>> _fences;
    std::vector<am::CRcPtr<am::CSemaphore>> _image_acq;
    std::vector<am::CRcPtr<am::CSemaphore>> _graphics_done;

    // Frame counting
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
