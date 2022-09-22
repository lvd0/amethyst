#include <amethyst/core/file_view.hpp>

#include <amethyst/graphics/command_buffer.hpp>
#include <amethyst/graphics/async_texture.hpp>
#include <amethyst/graphics/typed_buffer.hpp>
#include <amethyst/graphics/context.hpp>

#include <amethyst/meta/constants.hpp>

#include <TaskScheduler.h>

#include <ktx.h>

namespace am {
    CAsyncTexture::CAsyncTexture() noexcept = default;

    CAsyncTexture::~CAsyncTexture() noexcept {
        AM_PROFILE_SCOPED();
        wait();
    }

    AM_NODISCARD CRcPtr<CAsyncTexture> CAsyncTexture::sync_make(CRcPtr<CDevice> device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto result = make(std::move(device), std::move(info));
        result->wait();
        return result;
    }

    AM_NODISCARD CRcPtr<CAsyncTexture> CAsyncTexture::make(CRcPtr<CDevice> device, SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(device->logger(), "CAsyncTexture requested, path: \"{}\"", info.path.generic_string());
        auto result = CRcPtr<Self>::make(new Self());
        result->_task = std::make_unique<enki::TaskSet>(
            1,
            [device, result = result.get(), data = std::move(info)](enki::TaskSetPartition, uint32 thread) mutable noexcept {
                AM_PROFILE_SCOPED();
                auto file = CFileView::make(data.path);
                if (!file) {
                    switch (file.error()) {
                        case CFileView::EErrorType::FileNotFound:
                            AM_LOG_ERROR(device->logger(), "CAsyncTexture, \"{}\": file not found", data.path.generic_string());
                            break;
                        case CFileView::EErrorType::InternalError:
                            AM_LOG_ERROR(device->logger(), "CAsyncTexture, \"{}\": internal error", data.path.generic_string());
                            break;
                    }
                    // TODO: Do something useful
                    return;
                }
                ktxTexture2* texture;
                AM_ASSERT(!ktxTexture2_CreateFromMemory(
                    (const uint8*)file->data(),
                    file->size(),
                    KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                    &texture), "KTX loading failure");
                AM_LIKELY_IF(ktxTexture2_NeedsTranscoding(texture)) {
                    AM_PROFILE_NAMED_SCOPE("transcode");
                    AM_LOG_WARN(device->logger(), "transcoding texture");
                    auto format = data.type == ETextureType::Color ? KTX_TTF_BC7_RGBA : KTX_TTF_BC5_RG;
                    AM_ASSERT(!ktxTexture2_TranscodeBasis(texture, format, KTX_TF_HIGH_QUALITY), "transcoding failure");
                }
                auto staging = CTypedBuffer<uint8>::make(device, {
                    .usage = EBufferUsage::TransferSRC,
                    .memory = am::memory_auto,
                    .capacity = texture->dataSize,
                    .staging = true
                });
                staging->insert(texture->pData, texture->dataSize);
                auto image = CImage::make(device, {
                    .queue = EQueueType::Transfer,
                    .samples = EImageSampleCount::s1,
                    .usage = EImageUsage::Sampled | EImageUsage::TransferDST,
                    .format = static_cast<EResourceFormat>(texture->vkFormat),
                    .layout = EImageLayout::Undefined,
                    .layers = 1,
                    .mips = texture->numLevels,
                    .width = texture->baseWidth,
                    .height = texture->baseHeight
                });
                auto transfer_cmds = CCommandBuffer::make(device, {
                    .queue = EQueueType::Transfer,
                    .pool = ECommandPoolType::Transient,
                    .index = thread
                });

                transfer_cmds->begin()
                    .transition_layout({
                        .image = image.get(),
                        .source_stage = EPipelineStage::TopOfPipe,
                        .dest_stage = EPipelineStage::Transfer,
                        .source_access = EResourceAccess::None,
                        .dest_access = EResourceAccess::TransferWrite,
                        .old_layout = EImageLayout::Undefined,
                        .new_layout = EImageLayout::TransferDSTOptimal,
                        .layer = all_layers,
                        .mip = all_mips
                    });
                for (uint32 mip = 0; mip < texture->numLevels; ++mip) {
                    uint64 offset;
                    ktxTexture_GetImageOffset(ktxTexture(texture), mip, 0, 0, &offset);
                    transfer_cmds->copy_buffer_to_image(staging->info(offset), image.get(), mip);
                }
                transfer_cmds->transfer_ownership(*device->transfer_queue(), *device->graphics_queue(), {
                    .image = image.get(),
                    .source_stage = EPipelineStage::Transfer,
                    .dest_stage = EPipelineStage::None,
                    .source_access = EResourceAccess::TransferWrite,
                    .dest_access = EResourceAccess::None,
                    .old_layout = EImageLayout::TransferDSTOptimal,
                    .new_layout = EImageLayout::ShaderReadOnlyOptimal,
                    .layer = all_layers,
                    .mip = all_mips
                }).end();
                auto transfer_done = CSemaphore::make(device);
                device->transfer_queue()->submit({ {
                    .stage_mask = EPipelineStage::None,
                    .command = transfer_cmds.get(),
                    .wait = nullptr,
                    .signal = transfer_done.get(),
                } }, nullptr);
                auto ownership_cmds = CCommandBuffer::make(device, {
                    .queue = EQueueType::Graphics,
                    .pool = ECommandPoolType::Transient,
                    .index = thread
                });
                ownership_cmds->begin();
                AM_UNLIKELY_IF(device->transfer_queue()->family() != device->graphics_queue()->family()) {
                    ownership_cmds->transfer_ownership(*device->transfer_queue(), *device->graphics_queue(), {
                        .image = image.get(),
                        .source_stage = EPipelineStage::None,
                        .dest_stage = EPipelineStage::FragmentShader,
                        .source_access = EResourceAccess::None,
                        .dest_access = EResourceAccess::ShaderRead,
                        .old_layout = EImageLayout::TransferDSTOptimal,
                        .new_layout = EImageLayout::ShaderReadOnlyOptimal,
                        .layer = all_layers,
                        .mip = all_mips
                    });
                }
                ownership_cmds->end();
                auto fence = CFence::make(device, false);
                device->graphics_queue()->submit({ {
                    .stage_mask = EPipelineStage::Transfer,
                    .command = ownership_cmds.get(),
                    .wait = transfer_done.get(),
                    .signal = nullptr,
                } }, fence.get());
                result->_handle = std::move(image);
                ktxTexture_Destroy(ktxTexture(texture));
                fence->wait();
            });
        device->context()->scheduler()->AddTaskSetToPipe(result->_task.get());

        result->_device = std::move(device);
        return result;
    }

    AM_NODISCARD const CImage* CAsyncTexture::handle() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle.get();
    }

    AM_NODISCARD bool CAsyncTexture::is_ready() const noexcept {
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

    void CAsyncTexture::wait() const noexcept {
        AM_PROFILE_SCOPED();
        AM_UNLIKELY_IF(_task) {
            _device->context()->scheduler()->WaitforTask(_task.get());
            _task.reset();
        }
    }
} // namespace am
