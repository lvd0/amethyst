#include <amethyst/graphics/command_buffer.hpp>
#include <amethyst/graphics/async_mesh.hpp>
#include <amethyst/graphics/context.hpp>
#include <amethyst/graphics/queue.hpp>

#include <meshoptimizer.h>

#include <TaskScheduler.h>

#include <numeric>
#include <cstring>
#include <vector>

namespace am {
    CAsyncMesh::CAsyncMesh() noexcept = default;

    CAsyncMesh::~CAsyncMesh() noexcept {
        AM_PROFILE_SCOPED();
        wait();
        _device->vertex_buffer_allocator()->free(std::move(_vertices));
        _device->index_buffer_allocator()->free(std::move(_indices));
    }

    AM_NODISCARD CRcPtr<CAsyncMesh> CAsyncMesh::sync_make(CRcPtr<CDevice> device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto result = make(std::move(device), std::move(info));
        result->wait();
        return result;
    }

    AM_NODISCARD CRcPtr<CAsyncMesh> CAsyncMesh::make(CRcPtr<CDevice> device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(device->logger(), "CAsyncMesh requested, vertices: {}, indices: {}", info.geometry.size(), info.indices.size());
        auto result = CRcPtr<Self>::make(new Self());
        result->_task = std::make_unique<enki::TaskSet>(
            1,
            [device, result = result.get(), data = std::move(info)](enki::TaskSetPartition, uint32 thread) mutable noexcept {
                AM_PROFILE_SCOPED();
                std::vector<float32> opt_geometry;
                std::vector<uint32> opt_indices;
                {
                    AM_PROFILE_NAMED_SCOPE("mesh loader: optimizing mesh");
                    AM_UNLIKELY_IF(data.indices.empty()) {
                        data.indices.resize(data.geometry.size());
                        std::iota(data.indices.begin(), data.indices.end(), 0);
                    }
                    const auto total_vertex = data.geometry.size() / prv::vertex_components;
                    std::vector<uint32> opt_remap(total_vertex);
                    const auto vertex_count = meshopt_generateVertexRemap(
                        opt_remap.data(),
                        data.indices.data(),
                        data.indices.size(),
                        data.geometry.data(),
                        total_vertex,
                        sizeof(prv::SVertex));

                    opt_indices.resize(data.indices.size());
                    opt_geometry.resize(vertex_count * prv::vertex_components);
                    meshopt_remapIndexBuffer(
                        opt_indices.data(),
                        data.indices.data(),
                        data.indices.size(),
                        opt_remap.data());
                    meshopt_remapVertexBuffer(
                        opt_geometry.data(),
                        data.geometry.data(),
                        total_vertex,
                        sizeof(prv::SVertex),
                        opt_remap.data());
                    meshopt_optimizeVertexCache(
                        opt_indices.data(),
                        opt_indices.data(),
                        data.indices.size(),
                        total_vertex);
                    meshopt_optimizeOverdraw(
                        opt_indices.data(),
                        opt_indices.data(),
                        data.indices.size(),
                        opt_geometry.data(),
                        total_vertex,
                        sizeof(prv::SVertex),
                        1.075f);
                    meshopt_optimizeVertexFetch(
                        opt_geometry.data(),
                        opt_indices.data(),
                        data.indices.size(),
                        opt_geometry.data(),
                        vertex_count,
                        sizeof(prv::SVertex));
                }
                auto vertex_staging = CTypedBuffer<float32>::make(device, {
                    .usage = EBufferUsage::TransferSRC,
                    .memory = memory_auto,
                    .capacity = opt_geometry.size(),
                    .staging = true
                });
                vertex_staging->insert(opt_geometry);
                auto vertex_dest = device->vertex_buffer_allocator()->allocate(vertex_staging->size_bytes());

                auto index_staging = CTypedBuffer<uint32>::make(device, {
                    .usage = EBufferUsage::TransferSRC,
                    .memory = memory_auto,
                    .capacity = opt_indices.size(),
                    .staging = true
                });
                index_staging->insert(opt_indices);
                auto index_dest = device->index_buffer_allocator()->allocate(index_staging->size_bytes());

                auto transfer_cmds = CCommandBuffer::make(device, {
                    .queue = EQueueType::Transfer,
                    .pool = ECommandPoolType::Transient,
                    .index = thread
                });
                transfer_cmds->begin()
                    .copy_buffer(vertex_staging->info(), vertex_dest.handle->info(vertex_dest.offset))
                    .copy_buffer(index_staging->info(), index_dest.handle->info(index_dest.offset))
                    .end();
                auto fence = CFence::make(device, false);
                device->transfer_queue()->submit({ {
                    .stage_mask = EPipelineStage::None,
                    .command = transfer_cmds.get(),
                    .wait = nullptr,
                    .signal = nullptr,
                } }, fence.get());

                result->_vertices = vertex_dest;
                result->_indices = index_dest;
                fence->wait();
            });
        device->context()->scheduler()->AddTaskSetToPipe(result->_task.get());

        result->_device = std::move(device);
        return result;
    }

    AM_NODISCARD const SBufferSlice* CAsyncMesh::vertices() const noexcept {
        AM_PROFILE_SCOPED();
        return &_vertices;
    }

    AM_NODISCARD const SBufferSlice* CAsyncMesh::indices() const noexcept {
        AM_PROFILE_SCOPED();
        return &_indices;
    }

    AM_NODISCARD uint64 CAsyncMesh::vertex_offset() const noexcept {
        AM_PROFILE_SCOPED();
        return _vertices.offset / sizeof(prv::SVertex);
    }

    AM_NODISCARD uint64 CAsyncMesh::index_offset() const noexcept {
        AM_PROFILE_SCOPED();
        return _indices.offset / sizeof(uint32);
    }

    AM_NODISCARD bool CAsyncMesh::is_ready() const noexcept {
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

    void CAsyncMesh::wait() const noexcept {
        AM_PROFILE_SCOPED();
        AM_UNLIKELY_IF(_task) {
            _device->context()->scheduler()->WaitforTask(_task.get());
            _task.reset();
        }
    }
} // namespace am
