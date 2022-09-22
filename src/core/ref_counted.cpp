#include <amethyst/core/ref_counted.hpp>

namespace am {
    IRefCounted::IRefCounted() noexcept = default;

    uint64 IRefCounted::grab() const noexcept {
        AM_PROFILE_SCOPED();
        return ++_counter;
    }

    uint64 IRefCounted::drop() const noexcept {
        AM_PROFILE_SCOPED();
        return --_counter;
    }
} // namespace am
