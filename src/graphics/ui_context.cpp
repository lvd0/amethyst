#include <amethyst/graphics/descriptor_pool.hpp>
#include <amethyst/graphics/command_buffer.hpp>
#include <amethyst/graphics/async_texture.hpp>
#include <amethyst/graphics/render_pass.hpp>
#include <amethyst/graphics/framebuffer.hpp>
#include <amethyst/graphics/swapchain.hpp>
#include <amethyst/graphics/context.hpp>
#include <amethyst/graphics/device.hpp>
#include <amethyst/graphics/image.hpp>

#include <amethyst/graphics/ui_context.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <imgui.h>
#include <implot.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

// TODO: Horrible, horrible hack
struct ImGui_ImplVulkanH_WindowRenderBuffers {
    uint32_t  Index;
    uint32_t  Count;
    void*     FrameRenderBuffers;
};

struct ImGui_ImplVulkan_Data {
    ImGui_ImplVulkan_InitInfo   VulkanInitInfo;
    VkRenderPass                RenderPass;
    VkDeviceSize                BufferMemoryAlignment;
    VkPipelineCreateFlags       PipelineCreateFlags;
    VkDescriptorSetLayout       DescriptorSetLayout;
    VkPipelineLayout            PipelineLayout;
    VkPipeline                  Pipeline;
    uint32_t                    Subpass;
    VkShaderModule              ShaderModuleVert;
    VkShaderModule              ShaderModuleFrag;

    VkSampler                   FontSampler;
    VkDeviceMemory              FontMemory;
    VkImage                     FontImage;
    VkImageView                 FontView;
    VkDescriptorSet             FontDescriptorSet;
    VkDeviceMemory              UploadBufferMemory;
    VkBuffer                    UploadBuffer;

    ImGui_ImplVulkanH_WindowRenderBuffers MainWindowRenderBuffers;
};

namespace am {
    CUIContext::CUIContext() noexcept = default;

    CUIContext::~CUIContext() noexcept {
        AM_LOG_INFO(_logger, "terminating ui context");
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    AM_NODISCARD std::unique_ptr<CUIContext> CUIContext::make(SCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto* result = new Self();
        auto logger = spdlog::stdout_color_mt("ui_ctx");
        auto pool = CDescriptorPool::make(info.device);
        AM_LOG_INFO(logger, "initializing ui context");
        SAttachmentDescription main_attachment = {
            .format = { info.swapchain->format() },
            .samples = EImageSampleCount::s1,
            .layout = {},
            .clear = CClearValue::make()
        };
        if (info.load) {
            main_attachment.layout = {
                .initial = EImageLayout::ColorAttachmentOptimal,
                .final = EImageLayout::TransferSRCOptimal
            };
        } else {
            main_attachment.layout = {
                .initial = EImageLayout::Undefined,
                .final = EImageLayout::TransferSRCOptimal
            };
            main_attachment.clear = CClearValue::make({ .u32 = {} });
        }
        auto pass = CRenderPass::make(info.device, {
            .attachments = { main_attachment },
            .subpasses = { {
                .attachments = { 0 },
                .preserve = {},
                .input = {}
            } },
            .dependencies = { {
                .source_subpass = external_subpass,
                .dest_subpass = 0,
                .source_stage = EPipelineStage::ColorAttachmentOutput,
                .dest_stage = EPipelineStage::ColorAttachmentOutput,
                .source_access = EResourceAccess::ColorAttachmentWrite,
                .dest_access = EResourceAccess::ColorAttachmentWrite
            }, {
                .source_subpass = 0,
                .dest_subpass = external_subpass,
                .source_stage = EPipelineStage::ColorAttachmentOutput,
                .dest_stage = EPipelineStage::Transfer,
                .source_access = EResourceAccess::ColorAttachmentWrite,
                .dest_access = EResourceAccess::TransferRead
            } }
        });
        if (!info.load) {
            result->_framebuffer = CFramebuffer::make(info.device, {
                .width = info.swapchain->width(),
                .height = info.swapchain->height(),
                .attachments = {
                    make_attachment({
                        .index = 0,
                        .usage = EImageUsage::ColorAttachment | EImageUsage::TransferSRC
                    })
                },
                .pass = pass
            });
        }
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags =
            ImGuiConfigFlags_NavEnableKeyboard |
            ImGuiConfigFlags_DockingEnable;
        io.Fonts->AddFontFromFileTTF("data/fonts/verdana.ttf", 16);

        ImGui::StyleColorsDark();
        ImPlot::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowBorderSize = 0.0f;
        style.WindowRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.ChildBorderSize = 0.0f;
        style.FrameRounding = 0.0f;
        style.FrameBorderSize = 0.0f;
        ImGui_ImplGlfw_InitForVulkan(info.window->native(), true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = info.device->context()->native();
        init_info.PhysicalDevice = info.device->gpu();
        init_info.Device = info.device->native();
        init_info.QueueFamily = info.device->graphics_queue()->family();
        init_info.Queue = info.device->graphics_queue()->native();
        init_info.PipelineCache = nullptr;
        init_info.DescriptorPool = pool->fetch_pool();
        init_info.Subpass = 0;
        init_info.MinImageCount = info.swapchain->image_count();
        init_info.ImageCount = info.swapchain->image_count();
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = [](VkResult result) {
            AM_VULKAN_CHECK(spdlog::get("ui_ctx"), result);
        };
        ImGui_ImplVulkan_LoadFunctions([](const char* name, void* data) {
            return vkGetInstanceProcAddr((VkInstance)data, name);
        }, init_info.Instance);
        ImGui_ImplVulkan_Init(&init_info, pass->native());
        info.device->graphics_queue()->immediate_submit([](CCommandBuffer& commands) noexcept {
            ImGui_ImplVulkan_CreateFontsTexture(commands.native());
        });
        ImGui_ImplVulkan_DestroyFontUploadObjects();

        result->_logger = std::move(logger);
        result->_device = std::move(info.device);
        result->_swapchain = std::move(info.swapchain);
        result->_pool = std::move(pool);
        result->_pass = std::move(pass);
        return std::unique_ptr<Self>(result);
    }

    void CUIContext::bind_framebuffer(const CImage* image) noexcept {
        AM_PROFILE_SCOPED();
        if (!_framebuffer || _framebuffer->image(0) != image) {
            auto old = std::exchange(_framebuffer, CFramebuffer::make(_device, {
                .width = image->width(),
                .height = image->height(),
                .attachments = {
                    am::make_attachment({
                        .index = 0,
                        .image = image
                    }),
                },
                .pass = _pass
            }));
            if (old) {
                _device->cleanup_after(frames_in_flight + 1, [f0 = std::move(old)](const CDevice*) noexcept {});
            }
        }
    }

    void CUIContext::resize_framebuffer(uint32 width, uint32 height) noexcept {
        AM_PROFILE_SCOPED();
        _framebuffer->resize(width, height);
    }

    void CUIContext::acquire_frame() noexcept {
        AM_PROFILE_SCOPED();
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void CUIContext::render_frame(CCommandBuffer& commands, std::function<void()>&& ui) noexcept {
        AM_PROFILE_SCOPED();
        ui();
        ImGui::Render();
        commands.begin_render_pass(_framebuffer.get());
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commands.native());
        commands.end_render_pass();
    }

    AM_NODISCARD const CFramebuffer* CUIContext::framebuffer() const noexcept {
        AM_PROFILE_SCOPED();
        return _framebuffer.get();
    }

    AM_NODISCARD VkPipelineLayout CUIContext::pipeline_layout() noexcept {
        AM_PROFILE_SCOPED();
        const auto* backend = (ImGui_ImplVulkan_Data*)ImGui::GetIO().BackendRendererUserData;
        return backend->PipelineLayout;
    }

    AM_NODISCARD VkDescriptorSetLayout CUIContext::set_layout() noexcept {
        AM_PROFILE_SCOPED();
        const auto* backend = (ImGui_ImplVulkan_Data*)ImGui::GetIO().BackendRendererUserData;
        return backend->DescriptorSetLayout;
    }
} // namespace am
