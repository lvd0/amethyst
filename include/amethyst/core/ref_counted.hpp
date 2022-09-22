#pragma once

#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <atomic>

namespace am {
    class AM_MODULE IRefCounted {
    public:
        using Self = IRefCounted;

        uint64 grab() const noexcept;
        uint64 drop() const noexcept;

    protected:
        IRefCounted() noexcept;

    private:
        mutable std::atomic<uint64> _counter = 0;
    };
} // namespace am
