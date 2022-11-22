#pragma once

#include <amethyst/meta/macros.hpp>
#include <amethyst/meta/types.hpp>

#include <vulkan/vulkan.h>
#include <volk.h>

namespace am {
    enum class EClearValueType {
        None,
        Color,
        Depth
    };

    union SClearColor {
        float32 f32[4] = {};
        uint32 u32[4];
    };

    struct SClearDepth {
        float32 depth = 0;
        uint32 stencil = 0;
    };

    class AM_MODULE CClearValue {
    public:
        using Self = CClearValue;

        AM_CONSTEXPR ~CClearValue() noexcept;

        AM_NODISCARD AM_CONSTEXPR static Self make() noexcept;
        AM_NODISCARD AM_CONSTEXPR static Self make(SClearColor&&) noexcept;
        AM_NODISCARD AM_CONSTEXPR static Self make(SClearDepth&&) noexcept;

        AM_NODISCARD AM_CONSTEXPR VkClearValue native() const noexcept;
        AM_NODISCARD AM_CONSTEXPR EClearValueType type() const noexcept;

    private:
        AM_CONSTEXPR CClearValue() noexcept;

        VkClearValue _clear = {};
        EClearValueType _type = {};
    };

    AM_CONSTEXPR CClearValue::~CClearValue() noexcept = default;

    AM_CONSTEXPR CClearValue::CClearValue() noexcept = default;

    AM_NODISCARD AM_CONSTEXPR CClearValue CClearValue::make() noexcept {
        return {};
    }

    AM_NODISCARD AM_CONSTEXPR CClearValue CClearValue::make(SClearColor&& value) noexcept {
        Self result;
        result._clear.color = { {
            value.f32[0],
            value.f32[1],
            value.f32[2],
            value.f32[3]
        } };
        result._type = EClearValueType::Color;
        return result;
    }

    AM_NODISCARD AM_CONSTEXPR CClearValue CClearValue::make(SClearDepth&& value) noexcept {
        Self result;
        result._clear.depthStencil = { value.depth, value.stencil };
        result._type = EClearValueType::Depth;
        return result;
    }

    AM_NODISCARD AM_CONSTEXPR VkClearValue CClearValue::native() const noexcept {
        return _clear;
    }

    AM_NODISCARD AM_CONSTEXPR EClearValueType CClearValue::type() const noexcept {
        return _type;
    }
} // namespace am
