#include "native_market_access.hpp"

namespace ets2::native {
using cargo_filter::native::OwnershipStatus;

bool can_open_owned_cargo_market(const cargo_filter::native::OwnershipSnapshot& snapshot) noexcept {
    if (snapshot.status != OwnershipStatus::ready || !snapshot.player
        || !snapshot.player_vehicles || snapshot.trailers.empty()) return false;
    std::uintptr_t truck = 0;
    std::int32_t flags = 0;
    return safe_read(snapshot.player_vehicles + 0x10, truck) && truck
        && safe_read(truck + 8, flags) && flags < 0;
}

void CargoMenuAccess::refresh(const HostContext& host, bool force) {
    if (!host.module_base || GetCurrentThreadId() != host.main_thread_id) return;
    const auto now = GetTickCount64();
    if (!force && now < next_check_) return;
    next_check_ = now + 250;

    std::uintptr_t world = 0;
    std::int32_t flags = 0;
    if (!safe_read(host.address(0x36af838), world) || !world
        || !safe_read(world + 8, flags) || flags >= 0) return;
    const auto snapshot = cargo_filter::native::read_ownership(host.module_base);
    if (snapshot.status != OwnershipStatus::ready) return;

    // A different profile owns a fresh native action state. Never restore an
    // override from an earlier profile into it.
    if (world != enabled_world_ || snapshot.player != enabled_player_) {
        enabled_world_ = 0;
        enabled_player_ = 0;
    }
    const bool eligible = can_open_owned_cargo_market(snapshot);
    if (!eligible && !enabled_player_) return;
    const char* action = "jobs.cargo.market";
    const auto is_enabled = reinterpret_cast<bool (*)(std::uintptr_t, const char* const*)>(host.address(0x50ab00));
    const auto enable = reinterpret_cast<void (*)(std::uintptr_t, const char* const*)>(host.address(0x50a940));
    const auto disable = reinterpret_cast<void (*)(std::uintptr_t, const char* const*)>(host.address(0x50aa20));
    if (eligible) {
        if (!is_enabled(world, &action)) {
            // Native action mutation also refreshes the open menu's controls.
            // Do not unlock company pickup or external/World of Trucks actions.
            enable(world, &action);
            enabled_world_ = world;
            enabled_player_ = snapshot.player;
            if (host.log) host.log(LogLevel::info, "Cargo Market enabled for owned trailers without requiring an assigned trailer.");
        }
    } else {
        // Selling the last trailer must undo only the permission we added.
        // An assigned trailer remains governed by the game's original rules.
        if (!snapshot.active_trailer && is_enabled(world, &action)) disable(world, &action);
        enabled_world_ = 0;
        enabled_player_ = 0;
    }
}
} // namespace ets2::native
