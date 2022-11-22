#pragma once

#include <amethyst/graphics/async_texture.hpp>
#include <amethyst/graphics/typed_buffer.hpp>
#include <amethyst/graphics/async_model.hpp>
#include <amethyst/graphics/pipeline.hpp>

#include <amethyst/meta/forwards.hpp>
#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <type_traits>
#include <utility>
#include <vector>

namespace am {
    namespace prv {
        template <typename... Args>
        AM_NODISCARD constexpr std::size_t hash(std::size_t seed, Args&&... args) noexcept {
            return ((seed ^= std::hash<std::remove_cvref_t<Args>>()(args) + 0x9e3779b9 + (seed << 6) + (seed >> 2)), ...);
        }
    } // namespace am::prv
} // namespace am

namespace std {
#if defined(_WIN32)
    template <typename T>
    struct hash<T*> {
        constexpr size_t operator ()(const T* value) const noexcept {
            return (uintptr_t)value;
        }
    };
#endif

    template <typename T>
    struct hash<am::CRcPtr<T>> {
        constexpr size_t operator ()(const am::CRcPtr<T>& value) const noexcept {
            return am::prv::hash(0, value.get());
        }
    };

    template <typename T>
    struct hash<vector<T>> {
        constexpr size_t operator ()(const vector<T>& value) const noexcept {
            size_t seed = 0;
            for (const auto& each : value) {
                seed = am::prv::hash(seed, each);
            }
            return seed;
        }
    };

    template <typename T, typename U>
    struct hash<pair<T, U>> {
        constexpr size_t operator ()(const pair<T, U>& value) const noexcept {
            size_t seed = 0;
            seed = am::prv::hash(seed, value.first);
            seed = am::prv::hash(seed, value.second);
            return seed;
        }
    };

    AM_MAKE_HASHABLE(am::SDescriptorBinding, value, value.dynamic, value.index, value.count, value.type, value.stage);
    AM_MAKE_HASHABLE(am::SBufferInfo, value, value.handle, value.offset, value.size, value.address);
    AM_MAKE_HASHABLE(am::STextureInfo, value, value.handle, value.sampler, value.layout);
    AM_MAKE_HASHABLE(am::SSamplerInfo, value, value.filter, value.border_color, value.address_mode, value.reduction_mode, value.anisotropy);
    AM_MAKE_HASHABLE(am::STexturedMesh, value, value.geometry, value.albedo, value.normal, value.vertices, value.indices);
} // namespace std