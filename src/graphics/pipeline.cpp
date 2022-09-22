#include <amethyst/core/file_view.hpp>

#include <amethyst/graphics/framebuffer.hpp>
#include <amethyst/graphics/pipeline.hpp>

#include <shaderc/shaderc.hpp>

#include <spirv_glsl.hpp>
#include <spirv.hpp>

#include <algorithm>
#include <numeric>
#include <map>
#include <utility>

namespace am {
    namespace spvc = spirv_cross;
    namespace shc = shaderc;
    namespace fs = std::filesystem;

    class CShaderIncluder : public shc::CompileOptions::IncluderInterface {
    public:
        using Self = CShaderIncluder;
        using Super = shc::CompileOptions::IncluderInterface;

        CShaderIncluder(fs::path root) noexcept : _root(std::move(root)) {};
        ~CShaderIncluder() noexcept override = default;

        shaderc_include_result* GetInclude(const char* requested_source,
                                           shaderc_include_type,
                                           const char*,
                                           size_t) noexcept override {
            AM_PROFILE_SCOPED();
            auto path = _root / requested_source;
            auto file = CFileView::make(path);
            std::string content((const char*)file->data(), file->size());
            return new CIncludeResult(std::move(content), path.filename().generic_string());
        }

        void ReleaseInclude(shaderc_include_result* data) noexcept override {
            AM_PROFILE_SCOPED();
            delete static_cast<CIncludeResult*>(data);
        }
    private:
        class CIncludeResult : public shaderc_include_result {
        public:
            using Self = CIncludeResult;
            using Super = shaderc_include_result;

            CIncludeResult(std::string&& content, std::string&& filename) noexcept
                : Super(),
                  _content(std::move(content)),
                  _filename(std::move(filename)) {
                AM_PROFILE_SCOPED();
                Super::content = _content.c_str();
                Super::content_length = _content.size();
                Super::source_name = _filename.c_str();
                Super::source_name_length = _filename.size();
                Super::user_data = nullptr;
            };

            ~CIncludeResult() noexcept = default;

        private:
            std::string _content;
            std::string _filename;
        };
        fs::path _root;
    };

    AM_NODISCARD static inline std::vector<uint32> compile_shader(fs::path&& path, shaderc_shader_kind kind, spdlog::logger* logger) noexcept {
        AM_PROFILE_SCOPED();
        const auto s_path = path.string();
        auto file = CFileView::make(path);
        if (!file) {
            switch (file.error()) {
                case CFileView::EErrorType::FileNotFound:
                    logger->error("shader: \"{}\" cannot load, file not found", s_path);
                    return {};
                case CFileView::EErrorType::InternalError:
                    logger->error("shader: \"{}\" cannot load, internal filesystem error", s_path);
                    return {};
            }
        }

        shc::Compiler compiler;
        shc::CompileOptions options;
        options.SetGenerateDebugInfo();
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        options.SetSourceLanguage(shaderc_source_language_glsl);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        options.SetTargetSpirv(shaderc_spirv_version_1_5);
        options.SetIncluder(std::make_unique<CShaderIncluder>(path.parent_path()));
        auto spirv = compiler.CompileGlslToSpv(
            (const char*)file->data(),
            file->size(),
            kind,
            s_path.c_str(),
            options);
        if (spirv.GetCompilationStatus() != shaderc_compilation_status_success) {
            logger->error("shader compile failed:\n\"{}\"", spirv.GetErrorMessage());
            logger->flush();
            return {};
        }
        return { spirv.cbegin(), spirv.cend() };
    }

    static inline void process_resource(CDevice* device,
                                        std::map<uint64, std::vector<SDescriptorBinding>>& descriptor_layout,
                                        std::map<std::string, SDescriptorBinding>& descriptor_bindings,
                                        std::map<std::string, VkPushConstantRange>& push_constants,
                                        const spvc::CompilerGLSL& compiler,
                                        const auto& resources,
                                        VkDescriptorType descriptor,
                                        VkShaderStageFlags stage,
                                        bool push_constant = false) {
        if (push_constant) {
            if (!resources.empty()) {
                const auto& resource = resources[0];
                auto [ptr, miss] = push_constants.try_emplace(resource.name);
                auto& [_, value] = *ptr;
                if (!miss) {
                    value.stageFlags |= stage;
                } else {
                    const auto size = compiler.get_declared_struct_size(compiler.get_type(resource.type_id));
                    auto& current = push_constants[resource.name];
                    current.stageFlags = stage;
                    current.offset = 0;
                    current.size = (uint32)size;
                }
            }
            return;
        }
        for (const auto& each : resources) {
            const auto set = compiler.get_decoration(each.id, spv::DecorationDescriptorSet);
            const auto binding = compiler.get_decoration(each.id, spv::DecorationBinding);
            const auto& type = compiler.get_type(each.type_id);
            auto count = 1u;
            auto is_dynamic = false;
            if (type.basetype == spvc::SPIRType::SampledImage) {
                const bool is_array = !type.array.empty();
                is_dynamic = is_array && type.array[0] == 0;
                if (is_dynamic) {
                    const auto& limits = device->limits();
                    count = std::min<uint32>(limits.maxPerStageDescriptorSamplers, 4096);
                } else if (is_array) {
                    count = type.array[0];
                }
            }
            auto& layout = descriptor_layout[set];
            const auto found = std::find_if(layout.begin(), layout.end(), [binding](const auto& each) {
                return each.index == binding;
            });
            if (found != layout.end()) {
                found->stage |= stage;
            } else {
                descriptor_layout[set].emplace_back(
                    descriptor_bindings[each.name] = {
                        .dynamic = is_dynamic,
                        .index = binding,
                        .count = count,
                        .type = descriptor,
                        .stage = stage
                    });
            }
        }
    }

    CPipeline::CPipeline() noexcept = default;

    CPipeline::~CPipeline() noexcept {
        AM_PROFILE_SCOPED();
        AM_LOG_INFO(_device->logger(), "destroying pipeline: {}, layout: {}", (const void*)_handle, (const void*)_layout._pipeline);
        vkDestroyPipelineLayout(_device->native(), _layout._pipeline, nullptr);
        vkDestroyPipeline(_device->native(), _handle, nullptr);
    }

    AM_NODISCARD CRcPtr<CPipeline> CPipeline::make(CRcPtr<CDevice> device, SGraphicsCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto result = new Self();
        bool should_create_vertex = !info.vertex.empty();
        bool should_create_fragment = !info.fragment.empty();
        bool should_create_geometry = !info.geometry.empty();
        AM_LOG_INFO(device->logger(), "creating graphics pipeline:");
        AM_LOG_INFO(device->logger(), "- vertex: {}", should_create_vertex ? info.vertex.string() : "null");
        AM_LOG_INFO(device->logger(), "- fragment: {}", should_create_fragment ? info.fragment.string() : "null");
        AM_LOG_INFO(device->logger(), "- geometry: {}", should_create_geometry ? info.geometry.string() : "null");
        AM_ASSERT(should_create_vertex, "cannot create graphics pipeline without vertex shader");

        VkPipelineShaderStageCreateInfo vertex_stage = {};
        vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_stage.pName = "main";

        VkPipelineShaderStageCreateInfo geometry_stage = {};
        if (!info.geometry.empty()) {
            geometry_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            geometry_stage.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            geometry_stage.pName = "main";
        }

        VkPipelineShaderStageCreateInfo fragment_stage = {};
        if (!info.fragment.empty()) {
            fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragment_stage.pName = "main";
        }

        std::map<std::string, VkPushConstantRange> push_constants;
        std::map<uint64, std::vector<SDescriptorBinding>> descriptor_layout;

        { // Vertex Stage
            auto binary = compile_shader(std::move(info.vertex), shaderc_vertex_shader, device->logger());
            AM_ASSERT(!binary.empty(), "cannot create graphics pipeline without vertex shader");
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
            const auto resources = compiler.get_shader_resources();

            VkShaderModuleCreateInfo module_create_info = {};
            module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_create_info.codeSize = size_bytes(binary);
            module_create_info.pCode = binary.data();
            AM_VULKAN_CHECK(device->logger(), vkCreateShaderModule(device->native(), &module_create_info, nullptr, &vertex_stage.module));

            process_resource(
                device.get(),
                descriptor_layout,
                result->_bindings,
                push_constants,
                compiler,
                resources.push_constant_buffers,
                {},
                VK_SHADER_STAGE_VERTEX_BIT,
                true);
            process_resource(
                device.get(),
                descriptor_layout,
                result->_bindings,
                push_constants,
                compiler,
                resources.uniform_buffers,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT);
            process_resource(
                device.get(),
                descriptor_layout,
                result->_bindings,
                push_constants,
                compiler,
                resources.storage_buffers,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT);
        }

        if (should_create_geometry) { // Geometry Stage
            auto binary = compile_shader(std::move(info.geometry), shaderc_geometry_shader, device->logger());
            if (binary.empty()) {
                should_create_geometry = false;
            } else {
                const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
                const auto resources = compiler.get_shader_resources();

                VkShaderModuleCreateInfo module_create_info = {};
                module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                module_create_info.codeSize = size_bytes(binary);
                module_create_info.pCode = binary.data();
                AM_VULKAN_CHECK(device->logger(), vkCreateShaderModule(device->native(), &module_create_info, nullptr, &geometry_stage.module));
            }
        }

        std::vector<VkPipelineColorBlendAttachmentState> attachment_outputs;
        if (should_create_fragment) { // Fragment Stage
            auto binary = compile_shader(std::move(info.fragment), shaderc_fragment_shader, device->logger());
            if (binary.empty()) {
                should_create_fragment = false;
            } else {
                const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
                const auto resources = compiler.get_shader_resources();

                VkShaderModuleCreateInfo module_create_info = {};
                module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                module_create_info.codeSize = size_bytes(binary);
                module_create_info.pCode = binary.data();
                AM_VULKAN_CHECK(device->logger(), vkCreateShaderModule(device->native(), &module_create_info, nullptr, &fragment_stage.module));

                VkPipelineColorBlendAttachmentState attachment = {};
                attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attachment.colorBlendOp = VK_BLEND_OP_ADD;
                attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                attachment.alphaBlendOp = VK_BLEND_OP_ADD;
                for (uint32 i = 0; const auto& outputs : resources.stage_outputs) {
                    const auto& type = compiler.get_type(outputs.type_id);
                    if (info.attachments.empty()) {
                        attachment.blendEnable = type.vecsize == 4;
                    } else {
                        switch (info.attachments[i++]) {
                            case EAttachmentBlend::Auto:
                                attachment.blendEnable = type.vecsize == 4;
                                break;

                            case EAttachmentBlend::Disabled:
                                attachment.blendEnable = false;
                                break;
                        }
                    }
                    switch (type.vecsize) {
                        case 4: attachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT; AM_FALLTHROUGH;
                        case 3: attachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT; AM_FALLTHROUGH;
                        case 2: attachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT; AM_FALLTHROUGH;
                        case 1: attachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
                    }
                }
                attachment_outputs.resize(resources.stage_outputs.size(), attachment);

                process_resource(
                    device.get(),
                    descriptor_layout,
                    result->_bindings,
                    push_constants,
                    compiler,
                    resources.push_constant_buffers,
                    {},
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    true);
                process_resource(
                    device.get(),
                    descriptor_layout,
                    result->_bindings,
                    push_constants,
                    compiler,
                    resources.uniform_buffers,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    VK_SHADER_STAGE_FRAGMENT_BIT);
                process_resource(
                    device.get(),
                    descriptor_layout,
                    result->_bindings,
                    push_constants,
                    compiler,
                    resources.storage_buffers,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VK_SHADER_STAGE_FRAGMENT_BIT);
                process_resource(
                    device.get(),
                    descriptor_layout,
                    result->_bindings,
                    push_constants,
                    compiler,
                    resources.sampled_images,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    VK_SHADER_STAGE_FRAGMENT_BIT);
                process_resource(
                    device.get(),
                    descriptor_layout,
                    result->_bindings,
                    push_constants,
                    compiler,
                    resources.storage_images,
                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    VK_SHADER_STAGE_FRAGMENT_BIT);
            }
        }

        VkVertexInputBindingDescription vertex_binding_description = {};
        vertex_binding_description.binding = 0;
        vertex_binding_description.stride =
            std::accumulate(
                info.attributes.begin(), info.attributes.end(), 0u,
                [](const auto value, const auto attribute) noexcept {
                    return value + static_cast<std::underlying_type_t<EVertexAttribute>>(attribute);
                });
        vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> vertex_attribute_descriptions;
        vertex_attribute_descriptions.reserve(info.attributes.size());
        for (uint32 offset = 0, location = 0; const auto attribute : info.attributes) {
            vertex_attribute_descriptions.push_back({
                .location = location++,
                .binding = 0,
                .format = [&]() noexcept {
                    switch (attribute) {
                        case EVertexAttribute::Vec1: return VK_FORMAT_R32_SFLOAT;
                        case EVertexAttribute::Vec2: return VK_FORMAT_R32G32_SFLOAT;
                        case EVertexAttribute::Vec3: return VK_FORMAT_R32G32B32_SFLOAT;
                        case EVertexAttribute::Vec4: return VK_FORMAT_R32G32B32A32_SFLOAT;
                    }
                    AM_UNREACHABLE();
                }(),
                .offset = offset
            });
            offset += static_cast<std::underlying_type_t<EVertexAttribute>>(attribute);
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_state = {};
        vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        if (!vertex_attribute_descriptions.empty()) {
            vertex_input_state.vertexBindingDescriptionCount = 1;
            vertex_input_state.pVertexBindingDescriptions = &vertex_binding_description;
            vertex_input_state.vertexAttributeDescriptionCount = (uint32)vertex_attribute_descriptions.size();
            vertex_input_state.pVertexAttributeDescriptions = vertex_attribute_descriptions.data();
        }

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {};
        input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state.primitiveRestartEnable = false;

        const auto fb_viewport = info.framebuffer->viewport();
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = (float32)fb_viewport.width;
        viewport.height = (float32)fb_viewport.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = {};
        scissor.extent = fb_viewport;

        VkPipelineViewportStateCreateInfo viewport_state = {};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer_state = {};
        rasterizer_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer_state.depthClampEnable = false;
        rasterizer_state.rasterizerDiscardEnable = false;
        rasterizer_state.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer_state.cullMode = prv::as_vulkan(info.cull);
        rasterizer_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer_state.depthBiasEnable = false;
        rasterizer_state.depthBiasConstantFactor = 0.0f;
        rasterizer_state.depthBiasClamp = 0.0f;
        rasterizer_state.depthBiasSlopeFactor = 0.0f;
        rasterizer_state.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling_state = {};
        multisampling_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling_state.sampleShadingEnable = true;
        multisampling_state.minSampleShading = 0.2f;
        multisampling_state.pSampleMask = nullptr;
        multisampling_state.alphaToCoverageEnable = false;
        multisampling_state.alphaToOneEnable = true;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};
        depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state.depthTestEnable = info.depth_test;
        depth_stencil_state.depthWriteEnable = info.depth_write;
        depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depth_stencil_state.depthBoundsTestEnable = false;
        depth_stencil_state.stencilTestEnable = false;
        depth_stencil_state.front = {};
        depth_stencil_state.back = {};
        depth_stencil_state.minDepthBounds = 0.0f;
        depth_stencil_state.maxDepthBounds = 1.0f;

        VkPipelineColorBlendStateCreateInfo color_blend_state = {};
        color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state.logicOpEnable = false;
        color_blend_state.logicOp = VK_LOGIC_OP_NO_OP;
        color_blend_state.attachmentCount = (uint32)attachment_outputs.size();
        color_blend_state.pAttachments = attachment_outputs.data();
        color_blend_state.blendConstants[0] = 0.0f;
        color_blend_state.blendConstants[1] = 0.0f;
        color_blend_state.blendConstants[2] = 0.0f;
        color_blend_state.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamic_states;
        dynamic_states.reserve(info.states.size());
        for (const auto state : info.states) {
            dynamic_states.emplace_back(prv::as_vulkan(state));
        }
        VkPipelineDynamicStateCreateInfo pipeline_dynamic_states = {};
        pipeline_dynamic_states.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pipeline_dynamic_states.dynamicStateCount = (uint32)dynamic_states.size();
        pipeline_dynamic_states.pDynamicStates = dynamic_states.data();
        
        std::vector<VkDescriptorSetLayout> set_layouts;
        set_layouts.reserve(descriptor_layout.size());
        result->_layout._set.reserve(descriptor_layout.size());
        for (const auto& [_, descriptors] : descriptor_layout) {
            SDescriptorSetLayout layout = {};
            layout.handle = device->acquire_cached_item(descriptors);
            AM_UNLIKELY_IF(!layout.handle) {
                std::vector<VkDescriptorBindingFlags> flags;
                flags.reserve(descriptors.size());
                std::vector<VkDescriptorSetLayoutBinding> bindings;
                bindings.reserve(descriptors.size());
                for (const auto& binding : descriptors) {
                    flags.emplace_back();
                    AM_UNLIKELY_IF(binding.dynamic) {
                        layout.dynamic = true;
                        layout.binds = binding.count;
                        flags.back() =
                            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
                    }
                    bindings.push_back({
                        .binding = binding.index,
                        .descriptorType = binding.type,
                        .descriptorCount = binding.count,
                        .stageFlags = binding.stage,
                        .pImmutableSamplers = nullptr
                    });
                }

                VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags = {};
                binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                binding_flags.bindingCount = (uint32)flags.size();
                binding_flags.pBindingFlags = flags.data();

                VkDescriptorSetLayoutCreateInfo layout_info = {};
                layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layout_info.pNext = &binding_flags;
                layout_info.bindingCount = (uint32)bindings.size();
                layout_info.pBindings = bindings.data();
                AM_VULKAN_CHECK(device->logger(), vkCreateDescriptorSetLayout(device->native(), &layout_info, nullptr, &layout.handle));
                device->set_cached_item(descriptors, layout.handle);
            }
            result->_layout._set.emplace_back(layout);
            set_layouts.emplace_back(layout.handle);
        }

        std::vector<VkPushConstantRange> push_constant_ranges;
        push_constant_ranges.reserve(push_constants.size());
        for (const auto& [_, value] : push_constants) {
            push_constant_ranges.emplace_back(value);
        }

        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = (uint32)set_layouts.size();
        pipeline_layout_info.pSetLayouts = set_layouts.data();
        pipeline_layout_info.pushConstantRangeCount = (uint32)push_constant_ranges.size();
        pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();
        AM_VULKAN_CHECK(device->logger(), vkCreatePipelineLayout(device->native(), &pipeline_layout_info, nullptr, &result->_layout._pipeline));

        std::vector<VkPipelineShaderStageCreateInfo> pipeline_stages;
        pipeline_stages.reserve(3);
        pipeline_stages.emplace_back(vertex_stage);
        if (should_create_geometry) { pipeline_stages.emplace_back(geometry_stage); }
        if (should_create_fragment) { pipeline_stages.emplace_back(fragment_stage); }

        VkGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = (uint32)pipeline_stages.size();
        pipeline_info.pStages = pipeline_stages.data();
        pipeline_info.pVertexInputState = &vertex_input_state;
        pipeline_info.pInputAssemblyState = &input_assembly_state;
        pipeline_info.pTessellationState = nullptr;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer_state;
        pipeline_info.pMultisampleState = &multisampling_state;
        pipeline_info.pDepthStencilState = &depth_stencil_state;
        pipeline_info.pColorBlendState = &color_blend_state;
        pipeline_info.pDynamicState = &pipeline_dynamic_states;
        pipeline_info.layout = result->_layout._pipeline;
        pipeline_info.renderPass = info.framebuffer->render_pass()->native();
        pipeline_info.subpass = info.subpass;
        pipeline_info.basePipelineHandle = nullptr;
        pipeline_info.basePipelineIndex = -1;

        AM_VULKAN_CHECK(device->logger(), vkCreateGraphicsPipelines(device->native(), nullptr, 1, &pipeline_info, nullptr, &result->_handle));
        for (const auto& stage : pipeline_stages) {
            vkDestroyShaderModule(device->native(), stage.module, nullptr);
        }
        result->_type = EPipelineType::Graphics;
        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD CRcPtr<CPipeline> CPipeline::make(CRcPtr<CDevice> device, SComputeCreateInfo&& info) noexcept {
        AM_PROFILE_SCOPED();
        auto result = new Self();
        VkPipelineShaderStageCreateInfo compute_stage = {};
        compute_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute_stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_stage.pName = "main";
        AM_LOG_INFO(device->logger(), "creating compute pipeline:");
        AM_LOG_INFO(device->logger(), "- compute: {}", info.compute.string());

        auto binary = compile_shader(std::move(info.compute), shaderc_compute_shader, device->logger());
        AM_ASSERT(!binary.empty(), "cannot create compute pipeline without compute shader");
        const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
        const auto resources = compiler.get_shader_resources();

        VkShaderModuleCreateInfo module_create_info = {};
        module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        module_create_info.codeSize = size_bytes(binary);
        module_create_info.pCode = binary.data();
        AM_VULKAN_CHECK(device->logger(), vkCreateShaderModule(device->native(), &module_create_info, nullptr, &compute_stage.module));

        std::map<std::string, VkPushConstantRange> push_constants;
        std::map<uint64, std::vector<SDescriptorBinding>> descriptor_layout;
        process_resource(
            device.get(),
            descriptor_layout,
            result->_bindings,
            push_constants,
            compiler,
            resources.push_constant_buffers,
            {},
            VK_SHADER_STAGE_COMPUTE_BIT,
            true);
        process_resource(
            device.get(),
            descriptor_layout,
            result->_bindings,
            push_constants,
            compiler,
            resources.uniform_buffers,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_COMPUTE_BIT);
        process_resource(
            device.get(),
            descriptor_layout,
            result->_bindings,
            push_constants,
            compiler,
            resources.storage_buffers,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_COMPUTE_BIT);
        process_resource(
            device.get(),
            descriptor_layout,
            result->_bindings,
            push_constants,
            compiler,
            resources.sampled_images,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_COMPUTE_BIT);
        process_resource(
            device.get(),
            descriptor_layout,
            result->_bindings,
            push_constants,
            compiler,
            resources.storage_images,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_COMPUTE_BIT);

        std::vector<VkDescriptorSetLayout> set_layouts;
        set_layouts.reserve(descriptor_layout.size());
        result->_layout._set.reserve(descriptor_layout.size());
        for (const auto& [_, descriptors] : descriptor_layout) {
            SDescriptorSetLayout layout = {};
            layout.handle = device->acquire_cached_item(descriptors);
            AM_UNLIKELY_IF(!layout.handle) {
                std::vector<VkDescriptorBindingFlags> flags;
                flags.reserve(descriptors.size());
                std::vector<VkDescriptorSetLayoutBinding> bindings;
                bindings.reserve(descriptors.size());
                for (const auto& binding : descriptors) {
                    flags.emplace_back();
                    AM_UNLIKELY_IF(binding.dynamic) {
                        layout.dynamic = true;
                        layout.binds = binding.count;
                        flags.back() =
                            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
                    }
                    bindings.push_back({
                        .binding = binding.index,
                        .descriptorType = binding.type,
                        .descriptorCount = binding.count,
                        .stageFlags = binding.stage,
                        .pImmutableSamplers = nullptr
                    });
                }

                VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags = {};
                binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                binding_flags.bindingCount = (uint32)flags.size();
                binding_flags.pBindingFlags = flags.data();

                VkDescriptorSetLayoutCreateInfo layout_info = {};
                layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layout_info.pNext = &binding_flags;
                layout_info.bindingCount = (uint32)bindings.size();
                layout_info.pBindings = bindings.data();
                AM_VULKAN_CHECK(device->logger(), vkCreateDescriptorSetLayout(device->native(), &layout_info, nullptr, &layout.handle));
                device->set_cached_item(descriptors, layout.handle);
            }
            result->_layout._set.emplace_back(layout);
            set_layouts.emplace_back(layout.handle);
        }

        std::vector<VkPushConstantRange> push_constant_ranges;
        push_constant_ranges.reserve(push_constants.size());
        for (const auto& [_, value] : push_constants) {
            push_constant_ranges.emplace_back(value);
        }

        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = (uint32)set_layouts.size();
        pipeline_layout_info.pSetLayouts = set_layouts.data();
        pipeline_layout_info.pushConstantRangeCount = (uint32)push_constant_ranges.size();
        pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();
        AM_VULKAN_CHECK(device->logger(), vkCreatePipelineLayout(device->native(), &pipeline_layout_info, nullptr, &result->_layout._pipeline));

        VkComputePipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = compute_stage;
        pipeline_info.layout = result->_layout._pipeline;
        AM_VULKAN_CHECK(device->logger(), vkCreateComputePipelines(device->native(), nullptr, 1, &pipeline_info, nullptr, &result->_handle));
        vkDestroyShaderModule(device->native(), compute_stage.module, nullptr);

        result->_type = EPipelineType::Compute;
        result->_device = std::move(device);
        return CRcPtr<Self>::make(result);
    }

    AM_NODISCARD VkPipeline CPipeline::native() const noexcept {
        AM_PROFILE_SCOPED();
        return _handle;
    }

    AM_NODISCARD EPipelineType CPipeline::type() const noexcept {
        AM_PROFILE_SCOPED();
        return _type;
    }

    AM_NODISCARD VkPipelineLayout CPipeline::main_layout() const noexcept {
        AM_PROFILE_SCOPED();
        return _layout._pipeline;
    }

    AM_NODISCARD const SDescriptorBinding& CPipeline::bindings(const std::string& name) const noexcept {
        AM_PROFILE_SCOPED();
        return _bindings.at(name);
    }

    AM_NODISCARD SDescriptorSetLayout CPipeline::set_layout(uint32 index) const noexcept {
        AM_PROFILE_SCOPED();
        return _layout._set[index];
    }
} // namespace am
