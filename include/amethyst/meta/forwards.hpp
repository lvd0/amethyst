#pragma once

struct GLFWwindow;

namespace am {
    template <typename, typename>
    class CExpected;
    template <typename>
    class CRcPtr;
    class CFileView;
    class IRefCounted;

    class CClearValue;
    class CContext;
    class CDevice;
    class CFramebuffer;
    class CQueue;
    class CRenderPass;
    class CImage;
    class CImageView;
    class CSwapchain;
    class CCommandBuffer;
    class CFence;
    class CSemaphore;
    struct SDescriptorBinding;
    struct SDescriptorSetLayout;
    class CPipeline;
    struct SBufferInfo;
    template <typename>
    class CTypedBuffer;
    class CAsyncMesh;
    class CDescriptorPool;
    class CDescriptorSet;
    struct SSamplerInfo;
    struct STextureInfo;
    class CAsyncTexture;
    class CVirtualAllocator;
    class CRawBuffer;
    class CBufferSlice;
    class CUIContext;
    class CQueryPool;

    class CWindowingSystem;
    class CWindow;
    class CInput;

    namespace prv {
        template <typename T>
        class IDebugMarker;
    } // namespace am::prv
} // namespace am

namespace enki {
    class TaskSet;
    class TaskScheduler;
} // namespace enki
