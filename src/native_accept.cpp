#include "native_accept.hpp"
#include <array>
#include <algorithm>
#include <cstring>

namespace ets2::native {
namespace {
using Address = std::uintptr_t;
template<class F> F entry(const HostContext& host, std::size_t rva) noexcept {
    return reinterpret_cast<F>(host.address(rva));
}
bool live(Address unit) noexcept {
    std::int32_t flags = 0;
    return unit && safe_read(unit + 8, flags) && flags < 0;
}
// Values that determine cargo quantity, pay and route. Unit headers/allocators
// differ between a preview and the newly generated company-owned offer.
bool same_offer(Address first, Address second) noexcept {
    constexpr std::pair<std::size_t, std::size_t> fields[] = {
        {0x48, 4}, {0x4c, 2}, {0x50, 6}, {0x58, 8}, {0x60, 4},
        {0xc8, 1}, {0xcc, 4}, {0xd0, 4}, {0xd8, 1}
    };
    for (const auto& [offset, length] : fields) {
        std::array<std::byte, 32> left{}, right{};
        if (!safe_read_bytes(first + offset, left.data(), length)
            || !safe_read_bytes(second + offset, right.data(), length)
            || std::memcmp(left.data(), right.data(), length) != 0) return false;
    }
    Address a = 0, b = 0;
    std::uint32_t an = 0, bn = 0;
    if (!safe_read(first + 0x18, a) || !safe_read(second + 0x18, b)
        || !safe_read(first + 0x20, an) || !safe_read(second + 0x20, bn)
        || an != bn || an > 512) return false;
    std::array<char, 512> av{}, bv{};
    return safe_read_bytes(a, av.data(), an) && safe_read_bytes(b, bv.data(), bn)
        && std::memcmp(av.data(), bv.data(), an) == 0;
}
AttachResult attach_unsafe(const HostContext& host,
    const cargo_filter::native::OwnershipSnapshot& snapshot,
    Address trailer, Address company, Address row) {
    Address singleton = 0, player = 0, assigned = 0, truck = 0, assigned_truck = 0;
    Address game = 0, actor = 0, physical_truck = 0, definition = 0, old_offer = 0;
    std::uint32_t vehicle_kind = 3, assigned_kind = 3, minute = 0, expiry = 0;
    if (!safe_read(host.address(0x3046780), singleton) || singleton != snapshot.economy
        || !safe_read(singleton + 0x18, player) || player != snapshot.player
        || !safe_read(player + 0x68, vehicle_kind) || vehicle_kind != 0
        || !safe_read(player + 0x98, assigned_kind) || assigned_kind != 0
        || !safe_read(player + 0xa0, assigned) || !live(assigned)
        || !safe_read(snapshot.player_vehicles + 0x10, truck) || !live(truck)
        || !safe_read(assigned + 0x10, assigned_truck) || assigned_truck != truck
        || !live(trailer) || !safe_read(trailer + 0x1a0, definition) || !live(definition)
        || !live(company) || !safe_read(row + 8, old_offer) || !live(old_offer)
        || !safe_read(singleton + 0x19c, minute) || !safe_read(old_offer + 0x48, expiry)
        || expiry < minute
        || !safe_read(host.address(0x36af7c8), game) || !game
        || !safe_read(game + 0x31b0, actor) || !live(actor)
        || !safe_read(actor + 0x18, physical_truck) || !live(physical_truck))
        return AttachResult::unavailable;
    // Never take a trailer currently used by an employee, nor cancel an active
    // delivery. The ordinary accept guard remains responsible for its dialog.
    for (const auto offset : {0x28, 0x30, 0x38}) {
        Address job = 0;
        if (!safe_read(player + offset, job) || job) return AttachResult::busy;
    }
    if (entry<Address (*)(Address, Address, Address)>(host, 0x5fb390)(player, singleton, trailer))
        return AttachResult::busy;

    // Empty placement array is an explicit native path in 1405dbf00: derive
    // trailer/chain positions from the physical truck pose, then couple through
    // 1405dcc20. No garage teleport, clock advance, or raw world-pointer swap.
    const std::array<std::uint64_t, 5> placements{host.address(0x21eb538), 0, 0, 0, 0};
    entry<void (*)(Address, Address, int, Address, const void*, bool)>(host, 0x5f9840)(
        player, singleton, 0, trailer, placements.data(), true);
    entry<void (*)(Address, Address, int, bool)>(host, 0x5f9fe0)(player, singleton, 0, true);
    Address owned_now = 0, assigned_now = 0, actor_trailer = 0;
    std::uint8_t coupled = 0;
    if (!safe_read(snapshot.player_vehicles + 0x48, owned_now) || owned_now != trailer
        || !safe_read(assigned + 0x48, assigned_now) || assigned_now != trailer
        || !safe_read(assigned + 0x78, coupled) || !coupled
        || !safe_read(actor + 0xc8, actor_trailer) || !live(actor_trailer))
        return AttachResult::failed;

    // The attach routine regenerates the market before restoring its assigned
    // physical vehicle. Repeat this source after both paths are consistent, so
    // the fallback generator also sees the selected trailer.
    entry<void (*)(Address, Address)>(host, 0x7cfba0)(company, singleton);

    // The engine refreshes real company offers during the switch. Use its
    // corresponding offer so native acceptance consumes the correct seed and
    // owns every reference after this screen's preview batches are destroyed.
    Address data = 0, actual = 0;
    std::uint64_t count = 0;
    if (!safe_read(company + 0xa8, data) || !safe_read(company + 0xb0, count)
        || count > 10 || (count && !data)) return AttachResult::offer_changed;
    for (std::uint64_t i = 0; i < count; ++i) {
        Address candidate = 0;
        if (safe_read(data + i * 8, candidate) && live(candidate) && same_offer(old_offer, candidate)) {
            actual = candidate;
            break;
        }
    }
    if (!actual) return AttachResult::offer_changed;
    InterlockedIncrement(reinterpret_cast<volatile LONG*>(actual + 8));
    std::memcpy(reinterpret_cast<void*>(row + 8), &actual, sizeof(actual));
    const auto remaining = InterlockedDecrement(reinterpret_cast<volatile LONG*>(old_offer + 8));
    if ((remaining & 0x1ffffff) == 0) entry<void (*)(Address)>(host, 0xfbfa0)(old_offer);
    return AttachResult::ready;
}
}

bool same_cargo_offer(Address first, Address second) noexcept {
    return same_offer(first, second);
}

AttachResult attach_for_offer(const HostContext& host,
    const cargo_filter::native::OwnershipSnapshot& snapshot,
    Address trailer, Address company, Address row) noexcept {
    if (!host.module_base || GetCurrentThreadId() != host.main_thread_id
        || snapshot.status != cargo_filter::native::OwnershipStatus::ready
        || std::none_of(snapshot.trailers.begin(), snapshot.trailers.end(),
            [trailer](const auto& owned) { return owned.address == trailer; }))
        return AttachResult::unavailable;
    __try { return attach_unsafe(host, snapshot, trailer, company, row); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return AttachResult::failed; }
}
}
