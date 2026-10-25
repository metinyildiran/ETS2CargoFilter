#pragma once
#include "native_host.hpp"
#include "native_ownership.hpp"

namespace ets2::native {

// The original cargo provider still requires the player's own truck. Only
// the assigned-trailer requirement is relaxed; profile/truck checks remain.
bool can_open_owned_cargo_market(const cargo_filter::native::OwnershipSnapshot& snapshot) noexcept;

class CargoMenuAccess {
public:
    void refresh(const HostContext& host, bool force = false);
private:
    ULONGLONG next_check_ = 0;
    std::uintptr_t enabled_world_ = 0;
    std::uintptr_t enabled_player_ = 0;
};

} // namespace ets2::native
