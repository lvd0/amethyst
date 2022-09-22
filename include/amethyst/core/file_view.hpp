#pragma once

#include <amethyst/core/expected.hpp>
#include <amethyst/core/rc_ptr.hpp>

#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <filesystem>

namespace am {
    class AM_MODULE CFileView : public IRefCounted {
    public:
        using Self = CFileView;
        enum class EErrorType {
            FileNotFound,
            InternalError,
        };

        ~CFileView() noexcept;

        AM_NODISCARD static CExpected<CRcPtr<Self>> make(const std::filesystem::path&) noexcept;

        AM_NODISCARD const void* data() const noexcept;
        AM_NODISCARD uint64 size() const noexcept;

    private:
        CFileView() noexcept;

        void* _handle = nullptr;
        void* _mapping = nullptr;
        uint64 _size = 0;
        const void* _data = nullptr;
    };
} // namespace am
