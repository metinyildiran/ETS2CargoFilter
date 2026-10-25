#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ets2::cargo_filter::native {

// Exact 1.61.1.9 field/type descriptors, corroborated with the cargo generator.
// Call only after the native host's executable identity gate succeeds.
inline constexpr std::size_t economy_singleton_rva = 0x3046780;
inline constexpr std::size_t economy_size = 0x1df0;
inline constexpr std::size_t player_size = 0x440;
inline constexpr std::size_t player_vehicles_size = 0x80;
inline constexpr std::size_t trailer_size = 0x1d0;
inline constexpr std::size_t company_size = 0x118;

enum class OwnershipStatus { ready, no_profile, unavailable, changed };

struct OwnedTrailer {
    std::uintptr_t address = 0;
    std::uintptr_t definition = 0;
    std::string session_id;
    std::string label;
    // Actual trailer_definition.body_type, decoded from its native SCS token.
    // Empty when unavailable; type_name is display text, not a compatibility rule.
    std::string body_type;
    std::string type_name;
    std::string raw_license_plate;
    bool is_private = false;
};

struct OwnershipSnapshot {
    OwnershipStatus status = OwnershipStatus::unavailable;
    std::string error;
    std::uintptr_t economy = 0;
    std::uintptr_t player = 0;
    std::uintptr_t player_vehicles = 0;
    std::uintptr_t active_trailer = 0;
    std::vector<OwnedTrailer> trailers;
};

// Read-only synchronous snapshot. Addresses/session IDs are not persistent save
// identifiers and must never be reused after profile reload or trailer removal.
// Returned native pointers are borrowed, not retained references. Consume only
// in the same game-thread operation; never call engine functions from a worker.
[[nodiscard]] OwnershipSnapshot read_ownership(std::uintptr_t module_base);

} // namespace ets2::cargo_filter::native
