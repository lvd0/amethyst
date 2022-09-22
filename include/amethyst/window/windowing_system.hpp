#pragma once

#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <memory>
#include <vector>

namespace am {
    class AM_MODULE CWindowingSystem {
    public:
        using Self = CWindowingSystem;

        ~CWindowingSystem() noexcept;

        AM_NODISCARD static std::unique_ptr<Self> make() noexcept;

        AM_NODISCARD std::vector<const char*> surface_extensions() const noexcept;
        AM_NODISCARD float64 current_time() const noexcept;
        void poll_events() const noexcept;
        void wait_events() const noexcept;

        // TODO: Add something?

    private:
        CWindowingSystem() noexcept;

    };
} // namespace am
