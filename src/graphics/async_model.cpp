#include <amethyst/core/file_view.hpp>

#include <amethyst/graphics/async_model.hpp>
#include <amethyst/graphics/context.hpp>

#include <TaskScheduler.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <cgltf.h>

#include <limits>
#include <vector>
#include <queue>
#include <cmath>
#include <map>

namespace am {
    namespace fs = std::filesystem;

    CAsyncModel::CAsyncModel() noexcept = default;

    CAsyncModel::~CAsyncModel() noexcept {
        AM_PROFILE_SCOPED();
        wait();
    }

    AM_NODISCARD CRcPtr<CAsyncModel> CAsyncModel::sync_make(CRcPtr<CDevice> device, fs::path&& path) noexcept {
        AM_PROFILE_SCOPED();
        auto result = make(std::move(device), std::move(path));
        result->wait();
        return result;
    }

    AM_NODISCARD CRcPtr<CAsyncModel> CAsyncModel::make(CRcPtr<CDevice> device, fs::path&& path) noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(device->logger(), "CAsyncModel requested: {}", path.generic_string());
        auto result = CRcPtr<Self>::make(new Self());
        result->_task = std::make_unique<enki::TaskSet>(
            1,
            [device, result = result.get(), path = std::move(path)](enki::TaskSetPartition, uint32) mutable noexcept {
                AM_PROFILE_SCOPED();
                auto gltf = CFileView::make(path);
                cgltf_options options = {};
                options.memory.alloc_func = [](void*, cgltf_size size) noexcept {
                    return operator new[](size);
                };
                options.memory.free_func = [](void*, void* ptr) noexcept {
                    return operator delete[](ptr);
                };
                options.file.read = [](const struct cgltf_memory_options* mem,
                                       const struct cgltf_file_options*,
                                       const char* path,
                                       cgltf_size* size,
                                       void** data) {
                    auto file = am::CFileView::make(path);
                    *size = file->size();
                    *data = mem->alloc_func(nullptr, *size);
                    std::memcpy(*data, file->data(), *size);
                    return cgltf_result_success;
                };
                options.file.release = [](const struct cgltf_memory_options* mem,
                                          const struct cgltf_file_options*,
                                          void* data) {
                    mem->free_func(nullptr, data);
                };
                cgltf_data* model = nullptr;
                {
                    // TODO: Should not assert on this
                    AM_PROFILE_NAMED_SCOPE("model loader: gltf parse");
                    AM_ASSERT(cgltf_parse(&options, gltf->data(), gltf->size(), &model) == cgltf_result_success, "failed to parse model");
                }
                gltf.value().reset();
                cgltf_load_buffers(&options, model, path.generic_string().c_str());

                const auto base_path = path.parent_path();
                std::map<const char*, CRcPtr<CAsyncTexture>> textures;
                const auto import_texture = [&](const cgltf_texture* texture, ETextureType type) noexcept {
                    AM_LIKELY_IF(texture && !textures.contains(texture->image->uri)) {
                        textures[texture->image->uri] = CAsyncTexture::make(device, {
                            base_path / texture->image->uri,
                            type
                        });
                    }
                };
                for (uint32 i = 0; i < model->materials_count; ++i) {
                    const auto& material = model->materials[i];
                    import_texture(material.pbr_metallic_roughness.base_color_texture.texture, ETextureType::Color);
                    import_texture(material.normal_texture.texture, ETextureType::NonColor);
                }

                std::mutex guard;
                std::vector<std::unique_ptr<enki::TaskSet>> subtasks;
                subtasks.reserve(model->scene->nodes_count * 4);
                for (uint32 i = 0; i < model->scene->nodes_count; ++i) {
                    AM_PROFILE_NAMED_SCOPE("model loader: processing meshes");
                    std::queue<cgltf_node*> nodes;
                    nodes.push(model->scene->nodes[i]);
                    while (!nodes.empty()) {
                        const auto* node = nodes.front();
                        nodes.pop();
                        device->context()->scheduler()->AddTaskSetToPipe(subtasks.emplace_back(std::make_unique<enki::TaskSet>(
                            1,
                            [node, result, device, &guard, &textures](enki::TaskSetPartition, uint32) {
                                AM_LIKELY_IF(!node->mesh) {
                                    return;
                                }
                                for (uint32 j = 0; j < node->mesh->primitives_count; ++j) {
                                    const auto& primitive = node->mesh->primitives[j];
                                    const glm::vec3* position_ptr = nullptr;
                                    const glm::vec3* normal_ptr = nullptr;
                                    const glm::vec2* uv_ptr = nullptr;
                                    const glm::vec4* tangent_ptr = nullptr;
                                    uint64 vertex_count = 0;
                                    // Vertices
                                    for (uint32 k = 0; k < primitive.attributes_count; ++k) {
                                        const auto& attribute = primitive.attributes[k];
                                        const auto* accessor = attribute.data;
                                        const auto* buf_view = accessor->buffer_view;
                                        const auto* data_ptr = (const char*)buf_view->buffer->data;
                                        switch (attribute.type) {
                                            case cgltf_attribute_type_position:
                                                AM_ASSERT(accessor->type == cgltf_type_vec3, "position must be a vec3");
                                                vertex_count = accessor->count;
                                                position_ptr = (const glm::vec3*)(data_ptr + buf_view->offset + accessor->offset);
                                                break;

                                            case cgltf_attribute_type_normal:
                                                AM_ASSERT(accessor->type == cgltf_type_vec3, "normal must be a vec3");
                                                normal_ptr = (const glm::vec3*)(data_ptr + buf_view->offset + accessor->offset);
                                                break;

                                            case cgltf_attribute_type_texcoord:
                                                AM_ASSERT(accessor->type == cgltf_type_vec2, "uv must be a vec2");
                                                uv_ptr = (const glm::vec2*)(data_ptr + buf_view->offset + accessor->offset);
                                                break;

                                            case cgltf_attribute_type_tangent:
                                                AM_ASSERT(accessor->type == cgltf_type_vec4, "tangent must be a vec4");
                                                tangent_ptr = (const glm::vec4*)(data_ptr + buf_view->offset + accessor->offset);
                                                break;

                                            default: break;
                                        }
                                    }
                                    AM_ASSERT(vertex_count != 0, "cannot load model with 0 vertices");
                                    std::vector<float32> vertices;
                                    vertices.resize(
                                        vertex_count * 3 +
                                        vertex_count * 3 +
                                        vertex_count * 2 +
                                        vertex_count * 4);
                                    auto min_aabb = glm::vec3(std::numeric_limits<float32>::max());
                                    auto max_aabb = glm::vec3(std::numeric_limits<float32>::min());
                                    {
                                        auto* ptr = vertices.data();
                                        for (uint32 v = 0; v < vertex_count; ++v, ptr += prv::vertex_components) {
                                            prv::SVertex vertex = {};
                                            AM_LIKELY_IF(position_ptr) {
                                                std::memcpy(&vertex.position, position_ptr + v, sizeof(glm::vec3));
                                            }
                                            AM_LIKELY_IF(normal_ptr) {
                                                std::memcpy(&vertex.normal, normal_ptr + v, sizeof(glm::vec3));
                                            }
                                            AM_LIKELY_IF(uv_ptr) {
                                                std::memcpy(&vertex.uv, uv_ptr + v, sizeof(glm::vec2));
                                            }
                                            AM_LIKELY_IF(tangent_ptr) {
                                                std::memcpy(&vertex.tangent, tangent_ptr + v, sizeof(glm::vec4));
                                            }
                                            std::memcpy(ptr, &vertex, sizeof vertex);
                                        }

                                        for (uint32 i = 0; i < vertex_count; ++i) {
                                            const auto index = prv::vertex_components * i;
                                            min_aabb.x = std::min(min_aabb.x, vertices[index + 0]);
                                            min_aabb.y = std::min(min_aabb.y, vertices[index + 1]);
                                            min_aabb.z = std::min(min_aabb.z, vertices[index + 2]);
                                            max_aabb.x = std::max(max_aabb.x, vertices[index + 0]);
                                            max_aabb.y = std::max(max_aabb.y, vertices[index + 1]);
                                            max_aabb.z = std::max(max_aabb.z, vertices[index + 2]);
                                        }
                                    }

                                    std::vector<uint32> indices;
                                    {
                                        const auto* accessor = primitive.indices;
                                        const auto* buf_view = accessor->buffer_view;
                                        const auto* data_ptr = (const char*)buf_view->buffer->data;
                                        indices.reserve(accessor->count);
                                        switch (accessor->component_type) {
                                            case cgltf_component_type_r_8:
                                            case cgltf_component_type_r_8u: {
                                                const auto* ptr = (const uint8*)(data_ptr + buf_view->offset + accessor->offset);
                                                std::copy(ptr, ptr + accessor->count, std::back_inserter(indices));
                                            } break;

                                            case cgltf_component_type_r_16:
                                            case cgltf_component_type_r_16u: {
                                                const auto* ptr = (const uint16*)(data_ptr + buf_view->offset + accessor->offset);
                                                std::copy(ptr, ptr + accessor->count, std::back_inserter(indices));
                                            } break;

                                            case cgltf_component_type_r_32f:
                                            case cgltf_component_type_r_32u: {
                                                const auto* ptr = (const uint32*)(data_ptr + buf_view->offset + accessor->offset);
                                                std::copy(ptr, ptr + accessor->count, std::back_inserter(indices));
                                            } break;

                                            default: AM_UNREACHABLE();
                                        }
                                    }
                                    std::lock_guard lock(guard);
                                    auto& submesh = result->_submeshes.emplace_back();
                                    submesh.vertices = (uint32)vertex_count;
                                    submesh.indices = (uint32)indices.size();
                                    submesh.geometry = CAsyncMesh::make(device, {
                                        .geometry = std::move(vertices),
                                        .indices = std::move(indices),
                                    });
                                    {
                                        const auto* base_color = primitive.material->pbr_metallic_roughness.base_color_factor;
                                        std::memcpy(glm::value_ptr(submesh.material.base_color), base_color, sizeof(float32[4]));
                                    }

                                    cgltf_node_transform_world(node, glm::value_ptr(submesh.transform));
                                    submesh.aabb.center = (max_aabb + min_aabb) / 2.0f;
                                    submesh.aabb.extents = {
                                        max_aabb.x - submesh.aabb.center.x,
                                        max_aabb.y - submesh.aabb.center.y,
                                        max_aabb.z - submesh.aabb.center.z
                                    };
                                    submesh.aabb.min = min_aabb;
                                    submesh.aabb.max = max_aabb;
                                    {
                                        const auto* material = primitive.material;
                                        const auto* texture = material->pbr_metallic_roughness.base_color_texture.texture;
                                        AM_LIKELY_IF(texture) {
                                            submesh.albedo = textures[texture->image->uri];
                                        }
                                        texture = material->normal_texture.texture;
                                        AM_LIKELY_IF(texture) {
                                            submesh.normal = textures[texture->image->uri];
                                        }
                                    }
                                }
                            })).get());
                        for (uint32 j = 0; j < node->children_count; ++j) {
                            nodes.push(node->children[j]);
                        }
                    }
                }
                {
                    AM_PROFILE_NAMED_SCOPE("model loader: cleaning up");
                    for (auto& task : subtasks) {
                        device->context()->scheduler()->WaitforTask(task.get(), enki::TASK_PRIORITY_HIGH);
                    }
                    cgltf_free(model);
                }
            });
        device->context()->scheduler()->AddTaskSetToPipe(result->_task.get());

        result->_device = std::move(device);
        return result;
    }

    AM_NODISCARD const std::vector<STexturedMesh>& CAsyncModel::submeshes() const noexcept {
        AM_PROFILE_SCOPED();
        return _submeshes;
    }

    AM_NODISCARD bool CAsyncModel::is_ready() const noexcept {
        AM_PROFILE_SCOPED();
        AM_LIKELY_IF(!_task) {
            return true;
        }
        AM_LIKELY_IF(_task->GetIsComplete()) {
            wait();
            return true;
        }
        return false;
    }

    void CAsyncModel::wait() const noexcept {
        AM_PROFILE_SCOPED();
        AM_UNLIKELY_IF(_task) {
            _device->context()->scheduler()->WaitforTask(_task.get());
            _task.reset();
        }
    }
} // namespace am