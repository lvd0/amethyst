#pragma once

#include <amethyst/meta/macros.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

namespace am {
    // VkSampleCountFlags
    enum class EImageSampleCount {
        s1 = VK_SAMPLE_COUNT_1_BIT,
        s2 = VK_SAMPLE_COUNT_2_BIT,
        s4 = VK_SAMPLE_COUNT_4_BIT,
        s8 = VK_SAMPLE_COUNT_8_BIT,
        s16 = VK_SAMPLE_COUNT_16_BIT,
        s32 = VK_SAMPLE_COUNT_32_BIT,
        s64 = VK_SAMPLE_COUNT_64_BIT
    };

    // VkImageUsageFlags
    enum class EImageUsage {
        TransferSRC = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        TransferDST = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        Sampled = VK_IMAGE_USAGE_SAMPLED_BIT,
        Storage = VK_IMAGE_USAGE_STORAGE_BIT,
        ColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        DepthStencilAttachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        TransientAttachment = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        InputAttachment = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VideoDecodeDST = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR,
        VideoDecodeSRC = VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR,
        VideoDecodeDPB = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,
        FragmentDensityMap = VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT,
        FragmentShadingRateAttachment = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
        VideoEncodeDST = VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR,
        VideoEncodeSRC = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR,
        VideoEncodeDPB = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR,
        InvocationMaskHUAWEI = VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI,
        ShadingRateImageNV = VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV
    };

    // VkFormat
    enum class EResourceFormat {
        Undefined = VK_FORMAT_UNDEFINED,
        R4G4UnormPack8 = VK_FORMAT_R4G4_UNORM_PACK8,
        R4G4B4A4UnormPack16 = VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        B4G4R4A4UnormPack16 = VK_FORMAT_B4G4R4A4_UNORM_PACK16,
        R5G6B5UnormPack16 = VK_FORMAT_R5G6B5_UNORM_PACK16,
        B5G6R5UnormPack16 = VK_FORMAT_B5G6R5_UNORM_PACK16,
        R5G5B5A1UnormPack16 = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
        B5G5R5A1UnormPack16 = VK_FORMAT_B5G5R5A1_UNORM_PACK16,
        A1R5G5B5UnormPack16 = VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        R8Unorm = VK_FORMAT_R8_UNORM,
        R8Snorm = VK_FORMAT_R8_SNORM,
        R8Uscaled = VK_FORMAT_R8_USCALED,
        R8Sscaled = VK_FORMAT_R8_SSCALED,
        R8Uint = VK_FORMAT_R8_UINT,
        R8Sint = VK_FORMAT_R8_SINT,
        R8Srgb = VK_FORMAT_R8_SRGB,
        R8G8Unorm = VK_FORMAT_R8G8_UNORM,
        R8G8Snorm = VK_FORMAT_R8G8_SNORM,
        R8G8Uscaled = VK_FORMAT_R8G8_USCALED,
        R8G8Sscaled = VK_FORMAT_R8G8_SSCALED,
        R8G8Uint = VK_FORMAT_R8G8_UINT,
        R8G8Sint = VK_FORMAT_R8G8_SINT,
        R8G8Srgb = VK_FORMAT_R8G8_SRGB,
        R8G8B8Unorm = VK_FORMAT_R8G8B8_UNORM,
        R8G8B8Snorm = VK_FORMAT_R8G8B8_SNORM,
        R8G8B8Uscaled = VK_FORMAT_R8G8B8_USCALED,
        R8G8B8Sscaled = VK_FORMAT_R8G8B8_SSCALED,
        R8G8B8Uint = VK_FORMAT_R8G8B8_UINT,
        R8G8B8Sint = VK_FORMAT_R8G8B8_SINT,
        R8G8B8Srgb = VK_FORMAT_R8G8B8_SRGB,
        B8G8R8Unorm = VK_FORMAT_B8G8R8_UNORM,
        B8G8R8Snorm = VK_FORMAT_B8G8R8_SNORM,
        B8G8R8Uscaled = VK_FORMAT_B8G8R8_USCALED,
        B8G8R8Sscaled = VK_FORMAT_B8G8R8_SSCALED,
        B8G8R8Uint = VK_FORMAT_B8G8R8_UINT,
        B8G8R8Sint = VK_FORMAT_B8G8R8_SINT,
        B8G8R8Srgb = VK_FORMAT_B8G8R8_SRGB,
        R8G8B8A8Unorm = VK_FORMAT_R8G8B8A8_UNORM,
        R8G8B8A8Snorm = VK_FORMAT_R8G8B8A8_SNORM,
        R8G8B8A8Uscaled = VK_FORMAT_R8G8B8A8_USCALED,
        R8G8B8A8Sscaled = VK_FORMAT_R8G8B8A8_SSCALED,
        R8G8B8A8Uint = VK_FORMAT_R8G8B8A8_UINT,
        R8G8B8A8Sint = VK_FORMAT_R8G8B8A8_SINT,
        R8G8B8A8Srgb = VK_FORMAT_R8G8B8A8_SRGB,
        B8G8R8A8Unorm = VK_FORMAT_B8G8R8A8_UNORM,
        B8G8R8A8Snorm = VK_FORMAT_B8G8R8A8_SNORM,
        B8G8R8A8Uscaled = VK_FORMAT_B8G8R8A8_USCALED,
        B8G8R8A8Sscaled = VK_FORMAT_B8G8R8A8_SSCALED,
        B8G8R8A8Uint = VK_FORMAT_B8G8R8A8_UINT,
        B8G8R8A8Sint = VK_FORMAT_B8G8R8A8_SINT,
        B8G8R8A8Srgb = VK_FORMAT_B8G8R8A8_SRGB,
        A8B8G8R8UnormPack32 = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        A8B8G8R8SnormPack32 = VK_FORMAT_A8B8G8R8_SNORM_PACK32,
        A8B8G8R8UscaledPack32 = VK_FORMAT_A8B8G8R8_USCALED_PACK32,
        A8B8G8R8SscaledPack32 = VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
        A8B8G8R8UintPack32 = VK_FORMAT_A8B8G8R8_UINT_PACK32,
        A8B8G8R8SintPack32 = VK_FORMAT_A8B8G8R8_SINT_PACK32,
        A8B8G8R8SrgbPack32 = VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        A2R10G10B10UnormPack32 = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        A2R10G10B10SnormPack32 = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
        A2R10G10B10UscaledPack32 = VK_FORMAT_A2R10G10B10_USCALED_PACK32,
        A2R10G10B10SscaledPack32 = VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
        A2R10G10B10UintPack32 = VK_FORMAT_A2R10G10B10_UINT_PACK32,
        A2R10G10B10SintPack32 = VK_FORMAT_A2R10G10B10_SINT_PACK32,
        A2B10G10R10UnormPack32 = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        A2B10G10R10SnormPack32 = VK_FORMAT_A2B10G10R10_SNORM_PACK32,
        A2B10G10R10UscaledPack32 = VK_FORMAT_A2B10G10R10_USCALED_PACK32,
        A2B10G10R10SscaledPack32 = VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
        A2B10G10R10UintPack32 = VK_FORMAT_A2B10G10R10_UINT_PACK32,
        A2B10G10R10SintPack32 = VK_FORMAT_A2B10G10R10_SINT_PACK32,
        R16Unorm = VK_FORMAT_R16_UNORM,
        R16Snorm = VK_FORMAT_R16_SNORM,
        R16Uscaled = VK_FORMAT_R16_USCALED,
        R16Sscaled = VK_FORMAT_R16_SSCALED,
        R16Uint = VK_FORMAT_R16_UINT,
        R16Sint = VK_FORMAT_R16_SINT,
        R16Sfloat = VK_FORMAT_R16_SFLOAT,
        R16G16Unorm = VK_FORMAT_R16G16_UNORM,
        R16G16Snorm = VK_FORMAT_R16G16_SNORM,
        R16G16Uscaled = VK_FORMAT_R16G16_USCALED,
        R16G16Sscaled = VK_FORMAT_R16G16_SSCALED,
        R16G16Uint = VK_FORMAT_R16G16_UINT,
        R16G16Sint = VK_FORMAT_R16G16_SINT,
        R16G16Sfloat = VK_FORMAT_R16G16_SFLOAT,
        R16G16B16Unorm = VK_FORMAT_R16G16B16_UNORM,
        R16G16B16Snorm = VK_FORMAT_R16G16B16_SNORM,
        R16G16B16Uscaled = VK_FORMAT_R16G16B16_USCALED,
        R16G16B16Sscaled = VK_FORMAT_R16G16B16_SSCALED,
        R16G16B16Uint = VK_FORMAT_R16G16B16_UINT,
        R16G16B16Sint = VK_FORMAT_R16G16B16_SINT,
        R16G16B16Sfloat = VK_FORMAT_R16G16B16_SFLOAT,
        R16G16B16A16Unorm = VK_FORMAT_R16G16B16A16_UNORM,
        R16G16B16A16Snorm = VK_FORMAT_R16G16B16A16_SNORM,
        R16G16B16A16Uscaled = VK_FORMAT_R16G16B16A16_USCALED,
        R16G16B16A16Sscaled = VK_FORMAT_R16G16B16A16_SSCALED,
        R16G16B16A16Uint = VK_FORMAT_R16G16B16A16_UINT,
        R16G16B16A16Sint = VK_FORMAT_R16G16B16A16_SINT,
        R16G16B16A16Sfloat = VK_FORMAT_R16G16B16A16_SFLOAT,
        R32Uint = VK_FORMAT_R32_UINT,
        R32Sint = VK_FORMAT_R32_SINT,
        R32Sfloat = VK_FORMAT_R32_SFLOAT,
        R32G32Uint = VK_FORMAT_R32G32_UINT,
        R32G32Sint = VK_FORMAT_R32G32_SINT,
        R32G32Sfloat = VK_FORMAT_R32G32_SFLOAT,
        R32G32B32Uint = VK_FORMAT_R32G32B32_UINT,
        R32G32B32Sint = VK_FORMAT_R32G32B32_SINT,
        R32G32B32Sfloat = VK_FORMAT_R32G32B32_SFLOAT,
        R32G32B32A32Uint = VK_FORMAT_R32G32B32A32_UINT,
        R32G32B32A32Sint = VK_FORMAT_R32G32B32A32_SINT,
        R32G32B32A32Sfloat = VK_FORMAT_R32G32B32A32_SFLOAT,
        R64Uint = VK_FORMAT_R64_UINT,
        R64Sint = VK_FORMAT_R64_SINT,
        R64Sfloat = VK_FORMAT_R64_SFLOAT,
        R64G64Uint = VK_FORMAT_R64G64_UINT,
        R64G64Sint = VK_FORMAT_R64G64_SINT,
        R64G64Sfloat = VK_FORMAT_R64G64_SFLOAT,
        R64G64B64Uint = VK_FORMAT_R64G64B64_UINT,
        R64G64B64Sint = VK_FORMAT_R64G64B64_SINT,
        R64G64B64Sfloat = VK_FORMAT_R64G64B64_SFLOAT,
        R64G64B64A64Uint = VK_FORMAT_R64G64B64A64_UINT,
        R64G64B64A64Sint = VK_FORMAT_R64G64B64A64_SINT,
        R64G64B64A64Sfloat = VK_FORMAT_R64G64B64A64_SFLOAT,
        B10G11R11UfloatPack32 = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        E5B9G9R9UfloatPack32 = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
        D16Unorm = VK_FORMAT_D16_UNORM,
        X8D24UnormPack32 = VK_FORMAT_X8_D24_UNORM_PACK32,
        D32Sfloat = VK_FORMAT_D32_SFLOAT,
        S8Uint = VK_FORMAT_S8_UINT,
        D16UnormS8Uint = VK_FORMAT_D16_UNORM_S8_UINT,
        D24UnormS8Uint = VK_FORMAT_D24_UNORM_S8_UINT,
        D32SfloatS8Uint = VK_FORMAT_D32_SFLOAT_S8_UINT,
        BC1RGBUnormBlock = VK_FORMAT_BC1_RGB_UNORM_BLOCK,
        BC1RGBBlockSrgb = VK_FORMAT_BC1_RGB_SRGB_BLOCK,
        BC1RGBAUnormBlock = VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        BC1RGBABlockSrgb = VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
        BC2UnormBlock = VK_FORMAT_BC2_UNORM_BLOCK,
        BC2BlockSrgb = VK_FORMAT_BC2_SRGB_BLOCK,
        BC3UnormBlock = VK_FORMAT_BC3_UNORM_BLOCK,
        BC3BlockSrgb = VK_FORMAT_BC3_SRGB_BLOCK,
        BC4UnormBlock = VK_FORMAT_BC4_UNORM_BLOCK,
        BC4SnormBlock = VK_FORMAT_BC4_SNORM_BLOCK,
        BC5UnormBlock = VK_FORMAT_BC5_UNORM_BLOCK,
        BC5SnormBlock = VK_FORMAT_BC5_SNORM_BLOCK,
        BC6HUfloatBlock = VK_FORMAT_BC6H_UFLOAT_BLOCK,
        BC6HSfloatBlock = VK_FORMAT_BC6H_SFLOAT_BLOCK,
        BC7UnormBlock = VK_FORMAT_BC7_UNORM_BLOCK,
        BC7BlockSrgb = VK_FORMAT_BC7_SRGB_BLOCK,
        ETC2R8G8B8UnormBlock = VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
        ETC2R8G8B8BlockSrgb = VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
        ETC2R8G8B8A1UnormBlock = VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
        ETC2R8G8B8A1BlockSrgb = VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
        ETC2R8G8B8A8UnormBlock = VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
        ETC2R8G8B8A8BlockSrgb = VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
        EACR11UnormBlock = VK_FORMAT_EAC_R11_UNORM_BLOCK,
        EACR11SnormBlock = VK_FORMAT_EAC_R11_SNORM_BLOCK,
        EACR11G11UnormBlock = VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
        EACR11G11SnormBlock = VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
        ASTC4x4UnormBlock = VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
        ASTC4x4BlockSrgb = VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
        ASTC5x4UnormBlock = VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
        ASTC5x4BlockSrgb = VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
        ASTC5x5UnormBlock = VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
        ASTC5x5BlockSrgb = VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
        ASTC6x5UnormBlock = VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
        ASTC6x5BlockSrgb = VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
        ASTC6x6UnormBlock = VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
        ASTC6x6BlockSrgb = VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
        ASTC8x5UnormBlock = VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
        ASTC8x5BlockSrgb = VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
        ASTC8x6UnormBlock = VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
        ASTC8x6BlockSrgb = VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
        ASTC8x8UnormBlock = VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
        ASTC8x8BlockSrgb = VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
        ASTC10x5UnormBlock = VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
        ASTC10x5BlockSrgb = VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
        ASTC10x6UnormBlock = VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
        ASTC10x6BlockSrgb = VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
        ASTC10x8UnormBlock = VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
        ASTC10x8BlockSrgb = VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
        ASTC10x10UnormBlock = VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
        ASTC10x10BlockSrgb = VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
        ASTC12x10UnormBlock = VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
        ASTC12x10BlockSrgb = VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
        ASTC12x12UnormBlock = VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
        ASTC12x12BlockSrgb = VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
        A4R4G4B4UnormPack16 = VK_FORMAT_A4R4G4B4_UNORM_PACK16,
        A4B4G4R4UnormPack16 = VK_FORMAT_A4B4G4R4_UNORM_PACK16,
        ASTC4x4SfloatBlock = VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK,
        ASTC5x4SfloatBlock = VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK,
        ASTC5x5SfloatBlock = VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK,
        ASTC6x5SfloatBlock = VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK,
        ASTC6x6SfloatBlock = VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK,
        ASTC8x5SfloatBlock = VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK,
        ASTC8x6SfloatBlock = VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK,
        ASTC8x8SfloatBlock = VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK,
        ASTC10x5SfloatBlock = VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK,
        ASTC10x6SfloatBlock = VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK,
        ASTC10x8SfloatBlock = VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK,
        ASTC10x10SfloatBlock = VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK,
        ASTC12x10SfloatBlock = VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK,
        ASTC12x12SfloatBlock = VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK,
    };

    // VkImageLayout
    enum class EImageLayout {
        Undefined = VK_IMAGE_LAYOUT_UNDEFINED,
        General = VK_IMAGE_LAYOUT_GENERAL,
        ColorAttachmentOptimal = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        DepthStencilAttachmentOptimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        DepthStencilReadOnlyOptimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        ShaderReadOnlyOptimal = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        TransferSRCOptimal = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        TransferDSTOptimal = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        Preinitialized = VK_IMAGE_LAYOUT_PREINITIALIZED,
        DepthReadOnlyStencilAttachmentOptimal = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
        DepthAttachmentStencilReadOnlyOptimal = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
        DepthAttachmentOptimal = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        DepthReadOnlyOptimal = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
        StencilAttachmentOptimal = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
        StencilReadOnlyOptimal = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
        ReadOnlyOptimal = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        AttachmentOptimal = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        PresentSRC = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VideoDecodeDST = VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR,
        VideoDecodeSRC = VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR,
        VideoDecodeDPB = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
        SharedPresent = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
        FragmentDensityMapOptimal = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
        FragmentShadingRateAttachmentOptimal = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
        VideoEncodeDST = VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR,
        VideoEncodeSRC = VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR,
        VideoEncodeDPB = VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR,
        ShadingRateOptimalNV = VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV,
    };

    // VkPipelineStageFlags
    enum class EPipelineStage {
        None = VK_PIPELINE_STAGE_NONE,
        TopOfPipe = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        DrawIndirect = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        VertexInput = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VertexShader = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        TessellationControlShader = VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
        TessellationEvaluationShader = VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
        GeometryShader = VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
        FragmentShader = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        EarlyFragmentTests = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        LateFragmentTests = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        ColorAttachmentOutput = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        ComputeShader = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        Transfer = VK_PIPELINE_STAGE_TRANSFER_BIT,
        BottomOfPipe = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        Host = VK_PIPELINE_STAGE_HOST_BIT,
        AllGraphics = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
        AllCommands = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        TransformFeedback = VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
        ConditionalRendering = VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT,
        AccelerationStructureBuild = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        RayTracingShader = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        TaskShaderNV = VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV,
        MeshShaderNV = VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV,
        FragmentDensityProcess = VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
        FragmentShadingRateAttachment = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
        CommandPreprocessNV = VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
        ShadingRateImage_NV = VK_PIPELINE_STAGE_SHADING_RATE_IMAGE_BIT_NV,
        RayTracingShaderNV = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV,
        AccelerationStructureBuildNV = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
    };

    // VkAccessFlags
    enum class EResourceAccess {
        None = VK_ACCESS_NONE,
        IndirectCommandRead = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        IndexRead = VK_ACCESS_INDEX_READ_BIT,
        VertexAttributeRead = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        UniformRead = VK_ACCESS_UNIFORM_READ_BIT,
        InputAttachmentRead = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
        ShaderRead = VK_ACCESS_SHADER_READ_BIT,
        ShaderWrite = VK_ACCESS_SHADER_WRITE_BIT,
        ColorAttachmentRead = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
        ColorAttachmentWrite = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        DepthStencilAttachmentRead = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        DepthStencilAttachmentWrite = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        TransferRead = VK_ACCESS_TRANSFER_READ_BIT,
        TransferWrite = VK_ACCESS_TRANSFER_WRITE_BIT,
        HostRead = VK_ACCESS_HOST_READ_BIT,
        HostWrite = VK_ACCESS_HOST_WRITE_BIT,
        MemoryRead = VK_ACCESS_MEMORY_READ_BIT,
        MemoryWrite = VK_ACCESS_MEMORY_WRITE_BIT,
        TransformFeedbackWrite = VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
        TransformFeedbackCounterRead = VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
        TransformFeedbackCounterWrite = VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
        ConditionalRenderingRead = VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
        ColorAttachmentReadNonCoherent = VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT,
        AccelerationStructureRead = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
        AccelerationStructureWrite = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        FragmentDensityMapRead = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,
        FragmentShadingRateAttachmentRead = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
        CommandPreprocessReadNV = VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV,
        CommandPreprocessWriteNV = VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV,
        ShadingRateImageReadNV = VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV,
        AccelerationStructureReadNV = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,
        AccelerationStructureWriteNV = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV,
    };

    // VkCullModeFlagBits
    enum class ECullMode {
        None = VK_CULL_MODE_NONE,
        Front = VK_CULL_MODE_FRONT_BIT,
        Back = VK_CULL_MODE_BACK_BIT,
        FrontAndBack = VK_CULL_MODE_FRONT_AND_BACK,
    };

    // VkDynamicState
    enum class EDynamicState {
        Viewport = VK_DYNAMIC_STATE_VIEWPORT,
        Scissor = VK_DYNAMIC_STATE_SCISSOR,
        LineWidth = VK_DYNAMIC_STATE_LINE_WIDTH,
        DepthBias = VK_DYNAMIC_STATE_DEPTH_BIAS,
        BlendConstants = VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        DepthBounds = VK_DYNAMIC_STATE_DEPTH_BOUNDS,
        StencilCompareMask = VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        StencilWriteMask = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        StencilReference = VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        CullMode = VK_DYNAMIC_STATE_CULL_MODE,
        FrontFace = VK_DYNAMIC_STATE_FRONT_FACE,
        PrimitiveTopology = VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
        ViewportWithCount = VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
        ScissorWithCount = VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
        VertexInputBindingStride = VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
        DepthTestEnable = VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        DepthWriteEnable = VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
        DepthCompareOp = VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
        DepthBoundsTestEnable = VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
        StencilTestEnable = VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
        StencilOp = VK_DYNAMIC_STATE_STENCIL_OP,
        RasterizerDiscardEnable = VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
        DepthBiasEnable = VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
        PrimitiveRestartEnable = VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
        ViewportWScalingNV = VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV,
        DiscardRectangle = VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT,
        SampleLocations = VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT,
        RayTracingPipelineStackSize = VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR,
        ViewportShadingRatePaletteNV = VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV,
        ViewportCoarseSampleOrderNV = VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV,
        ExclusiveScissorNV = VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV,
        FragmentShadingRate = VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,
        LineStipple = VK_DYNAMIC_STATE_LINE_STIPPLE_EXT,
        VertexInput = VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
        PatchControlPoints = VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT,
        LogicOp = VK_DYNAMIC_STATE_LOGIC_OP_EXT,
        ColorWriteEnable = VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT,
    };

    // VkBufferUsageFlags
    enum class EBufferUsage {
        TransferSRC = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        TransferDST = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        UniformTexelBuffer = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
        StorageTexelBuffer = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
        UniformBuffer = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        StorageBuffer = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        IndexBuffer = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VertexBuffer = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        IndirectBuffer = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        ShaderDeviceAddress = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VideoDecodeSRC = VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR,
        VideoDecodeDST = VK_BUFFER_USAGE_VIDEO_DECODE_DST_BIT_KHR,
        TransformFeedbackBuffer = VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT,
        TransformFeedbackCounterBuffer = VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT,
        ConditionalRendering = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT,
        AccelerationStructureBuildInputReadOnly = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        AccelerationStructureStorage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
        ShaderBindingTable = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
        VideoEncodeDST = VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR,
        VideoEncodeSRC = VK_BUFFER_USAGE_VIDEO_ENCODE_SRC_BIT_KHR,
        RayTracingNV = VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
    };

    // VkMemoryPropertyUsageFlags
    enum class EMemoryProperty {
        DeviceLocal = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        HostVisible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        HostCoherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        HostCached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
        LazilyAllocated = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT,
        Protected = VK_MEMORY_PROPERTY_PROTECTED_BIT,
        DeviceCoherentAMD = VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD,
        DeviceUncachedAMD = VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD,
        RDMACapableNV = VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV,
    };

    // VkFilter
    enum class EFilter {
        Nearest = VK_FILTER_NEAREST,
        Linear = VK_FILTER_LINEAR,
        CubicImg = VK_FILTER_CUBIC_IMG,
    };

    // VkSamplerMipmapMode
    enum class EMipMode {
        Nearest = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        Linear = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    };

    // VkBorderColor
    enum class EBorderColor {
        FloatTransparentBlack = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        IntTransparentBlack = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,
        FloatOpaqueBlack = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        IntOpaqueBlack = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        FloatOpaqueWhite = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        IntOpaqueWhite = VK_BORDER_COLOR_INT_OPAQUE_WHITE,
        FloatCustom = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT,
        IntCustom = VK_BORDER_COLOR_INT_CUSTOM_EXT,
    };

    // VkSamplerAddressMode
    enum class EAddressMode {
        Repeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        MirroredRepeat = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        ClampToEdge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        ClampToBorder = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        MirrorClampToEdge = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
    };

    // VkShaderStageFlags
    enum class EShaderStage {
        Vertex = VK_SHADER_STAGE_VERTEX_BIT,
        TessellationControl = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        TessellationEvaluation = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        Geometry = VK_SHADER_STAGE_GEOMETRY_BIT,
        Fragment = VK_SHADER_STAGE_FRAGMENT_BIT,
        Compute = VK_SHADER_STAGE_COMPUTE_BIT,
        AllGraphics = VK_SHADER_STAGE_ALL_GRAPHICS,
        All = VK_SHADER_STAGE_ALL,
        Raygen = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        AnyHit = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        ClosestHit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        Miss = VK_SHADER_STAGE_MISS_BIT_KHR,
        Intersection = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
        Callable = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        TaskNV = VK_SHADER_STAGE_TASK_BIT_NV,
        MeshNV = VK_SHADER_STAGE_MESH_BIT_NV,
        RaygenNV = VK_SHADER_STAGE_RAYGEN_BIT_NV,
        AnyHitNV = VK_SHADER_STAGE_ANY_HIT_BIT_NV,
        ClosestHitNV = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV,
        MissNV = VK_SHADER_STAGE_MISS_BIT_NV,
        IntersectionNV = VK_SHADER_STAGE_INTERSECTION_BIT_NV,
        CallableNV = VK_SHADER_STAGE_CALLABLE_BIT_NV,
    };

    // VkSamplerReductionMode
    enum class EReductionMode {
        WeightedAverage = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE,
        Min = VK_SAMPLER_REDUCTION_MODE_MIN,
        Max = VK_SAMPLER_REDUCTION_MODE_MAX,
    };

    // VkDescriptorType
    enum class EDescriptorType {
        Sampler = VK_DESCRIPTOR_TYPE_SAMPLER,
        CombinedImageSampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        SampledImage = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        StorageImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        UniformTexelBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        StorageTexelBuffer = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        UniformBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        StorageBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        UniformBufferDynamic = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        StorageBufferDynamic = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
        InputAttachment = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        InlineUniformBlock = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
        AccelerationStructure = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        AccelerationStructureNV = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV,
        MutableVALVE = VK_DESCRIPTOR_TYPE_MUTABLE_VALVE,
        SampleWeightImageQCOM = VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM,
        BlockMatchImageQCOM = VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM,
    };

    // VkQueryPipelineStatisticFlagBits
    enum class EQueryPipelineStatistics {
        InputAssemblyVertices = VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
        InputAssemblyPrimitives = VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
        VertexShaderInvocations = VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
        GeometryShaderInvocations = VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
        GeometryShaderPrimitives = VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
        ClippingInvocations = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
        ClippingPrimitives = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
        FragmentShaderInvocations = VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
        TessellationControlShaderPatches = VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
        TessellationEvaluationShaderInvocations = VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
        ComputeShaderInvocations = VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT,
    };

    // VkQueryType
    enum class EQueryType {
        Occlusion = VK_QUERY_TYPE_OCCLUSION,
        PipelineStatistics = VK_QUERY_TYPE_PIPELINE_STATISTICS,
        Timestamp = VK_QUERY_TYPE_TIMESTAMP,
        ResultStatusOnly = VK_QUERY_TYPE_RESULT_STATUS_ONLY_KHR,
        TransformFeedbackStream = VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT,
        PerformanceQuery = VK_QUERY_TYPE_PERFORMANCE_QUERY_KHR,
        AccelerationStructureCompactedSize = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
        AccelerationStructureSerializationSize = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR,
        AccelerationStructureCompactedSizeNV = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_NV,
        PerformanceQueryINTEL = VK_QUERY_TYPE_PERFORMANCE_QUERY_INTEL,
        VideoEncodeBitstreamBufferRange = VK_QUERY_TYPE_VIDEO_ENCODE_BITSTREAM_BUFFER_RANGE_KHR,
        PrimitivesGenerated = VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT,
        AccelerationStructureSerializationBottomLevelPointers = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_BOTTOM_LEVEL_POINTERS_KHR,
        AccelerationStructureSize = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SIZE_KHR
    };

    namespace prv {
        AM_NODISCARD constexpr VkImageAspectFlags deduce_aspect(VkFormat format) noexcept {
            switch (format) {
                case VK_FORMAT_S8_UINT:
                    return VK_IMAGE_ASPECT_STENCIL_BIT;
                case VK_FORMAT_D16_UNORM:
                case VK_FORMAT_X8_D24_UNORM_PACK32:
                case VK_FORMAT_D32_SFLOAT:
                    return VK_IMAGE_ASPECT_DEPTH_BIT;
                case VK_FORMAT_D16_UNORM_S8_UINT:
                case VK_FORMAT_D24_UNORM_S8_UINT:
                case VK_FORMAT_D32_SFLOAT_S8_UINT:
                    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
                default:
                    return VK_IMAGE_ASPECT_COLOR_BIT;
            }
            AM_UNREACHABLE();
        }

        constexpr const char* as_string(VkResult result) noexcept {
            switch (result) {
                case VK_SUCCESS: return "VK_SUCCESS";
                case VK_NOT_READY: return "VK_NOT_READY";
                case VK_TIMEOUT: return "VK_TIMEOUT";
                case VK_EVENT_SET: return "VK_EVENT_SET";
                case VK_EVENT_RESET: return "VK_EVENT_RESET";
                case VK_INCOMPLETE: return "VK_INCOMPLETE";
                case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
                case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
                case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
                case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
                case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
                case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
                case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
                case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
                case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
                case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
                case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
                case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
                case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
                case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
                case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
                case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
                case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
                case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
                case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
                case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
                case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
                case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
                case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
                case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
                case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
                case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
                case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
                case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR";
                case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
                case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
                case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
                case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
                case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
                case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
                default: return "unknown result";
            }
            AM_UNREACHABLE();
        }

#define AS_VULKAN_SPECIALIZATION(InT, OutT)                         \
        template <>                                                 \
        AM_NODISCARD constexpr auto as_vulkan(InT value) noexcept { \
            return static_cast<OutT>(value);                        \
        }

        template <typename E>
        AM_NODISCARD constexpr auto as_underlying(E e) noexcept {
            return static_cast<std::underlying_type_t<E>>(e);
        }

        template <typename E>
        AM_NODISCARD constexpr auto as_vulkan(E) noexcept;

        AS_VULKAN_SPECIALIZATION(EImageSampleCount, VkSampleCountFlagBits)
        AS_VULKAN_SPECIALIZATION(EImageUsage, VkImageUsageFlagBits)
        AS_VULKAN_SPECIALIZATION(EResourceFormat, VkFormat)
        AS_VULKAN_SPECIALIZATION(EImageLayout, VkImageLayout)
        AS_VULKAN_SPECIALIZATION(EPipelineStage, VkPipelineStageFlagBits)
        AS_VULKAN_SPECIALIZATION(EResourceAccess, VkAccessFlags)
        AS_VULKAN_SPECIALIZATION(ECullMode, VkCullModeFlagBits)
        AS_VULKAN_SPECIALIZATION(EDynamicState, VkDynamicState)
        AS_VULKAN_SPECIALIZATION(EBufferUsage, VkBufferUsageFlags)
        AS_VULKAN_SPECIALIZATION(EMemoryProperty, VkMemoryPropertyFlags)
        AS_VULKAN_SPECIALIZATION(EFilter, VkFilter)
        AS_VULKAN_SPECIALIZATION(EMipMode, VkSamplerMipmapMode)
        AS_VULKAN_SPECIALIZATION(EBorderColor, VkBorderColor)
        AS_VULKAN_SPECIALIZATION(EAddressMode, VkSamplerAddressMode)
        AS_VULKAN_SPECIALIZATION(EShaderStage, VkShaderStageFlagBits)
        AS_VULKAN_SPECIALIZATION(EReductionMode, VkSamplerReductionMode)
        AS_VULKAN_SPECIALIZATION(EDescriptorType, VkDescriptorType)
        AS_VULKAN_SPECIALIZATION(EQueryPipelineStatistics, VkQueryPipelineStatisticFlags)
        AS_VULKAN_SPECIALIZATION(EQueryType, VkQueryType)
#undef AS_VULKAN_SPECIALIZATION
    }
} // namespace am
