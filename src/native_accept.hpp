#pragma once
#include "native_host.hpp"
#include "native_ownership.hpp"

namespace ets2::native {
enum class AttachResult { ready, unavailable, busy, failed, offer_changed };
// Read-only identity check used before substituting a regenerated offer.
bool same_cargo_offer(std::uintptr_t first, std::uintptr_t second) noexcept;
// Main-thread only. Uses the engine to rebuild the physical trailer and market.
// Rebinds the selected row to the matching regenerated, company-owned offer.
AttachResult attach_for_offer(const HostContext& host,
    const cargo_filter::native::OwnershipSnapshot& snapshot,
    std::uintptr_t trailer, std::uintptr_t company, std::uintptr_t row) noexcept;
}
