#pragma once

#include "native_host.hpp"

namespace ets2::native {

enum class AppendRowResult { appended, skipped, failed };

// Calls run inside the provider's exclusive execution interval, before the
// native scheduler publishes/transfers its row vector. This class does not
// establish that interval, generate offers, or own the provider.
class NativeRows {
public:
    explicit NativeRows(const HostContext& host) noexcept : host_(host) {}

    // Call only once when the original provider did NOT execute its preparation
    // (for example, it returned early because no trailer was attached).
    [[nodiscard]] bool prepare_city_keys(std::uintptr_t task) const noexcept;

    // Original company is the real, retained world company, never a clone.
    // Offer must be a live native unit whose lifetime the caller already owns.
    // Native row copying acquires its own company and offer references.
    [[nodiscard]] AppendRowResult append(std::uintptr_t task,
        std::uintptr_t original_company, std::uintptr_t native_offer) const noexcept;

    [[nodiscard]] bool calculate_pay(std::uintptr_t task) const noexcept;

private:
    const HostContext& host_;
};

} // namespace ets2::native
