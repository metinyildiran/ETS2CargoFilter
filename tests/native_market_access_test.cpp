#include "native_market_access.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace ets2::native;
using namespace ets2::cargo_filter::native;
namespace {
int checks = 0;
void check(bool result, const char* message) {
    ++checks;
    if (!result) { std::cerr << message << '\n'; std::exit(1); }
}
template<class T> void put(std::byte* bytes, std::size_t offset, T value) {
    std::memcpy(bytes + offset, &value, sizeof(value));
}
}
int main() {
    OwnershipSnapshot snapshot;
    check(!can_open_owned_cargo_market(snapshot), "no loaded profile remains locked");
    alignas(8) std::array<std::byte, 0x80> vehicles{}, truck{};
    const auto truck_address = reinterpret_cast<std::uintptr_t>(truck.data());
    snapshot.status = OwnershipStatus::ready;
    snapshot.player = 1;
    snapshot.player_vehicles = reinterpret_cast<std::uintptr_t>(vehicles.data());
    put(vehicles.data(), 0x10, truck_address);
    put(truck.data(), 8, std::uint32_t{0x80000001});
    check(!can_open_owned_cargo_market(snapshot), "own truck but no trailers remains locked");
    snapshot.trailers.emplace_back().address = 42;
    check(can_open_owned_cargo_market(snapshot), "owned trailer unlocks market with no assigned trailer");
    snapshot.active_trailer = 42;
    check(can_open_owned_cargo_market(snapshot), "assigned owned trailer remains supported");
    snapshot.active_trailer = 0;
    put(vehicles.data(), 0x10, std::uintptr_t{0});
    check(!can_open_owned_cargo_market(snapshot), "no own truck must not reach native provider assertion");
    put(vehicles.data(), 0x10, truck_address);
    put(truck.data(), 8, std::uint32_t{1});
    check(!can_open_owned_cargo_market(snapshot), "expired truck cannot unlock market");
    put(truck.data(), 8, std::uint32_t{0x80000001});
    snapshot.status = OwnershipStatus::changed;
    check(!can_open_owned_cargo_market(snapshot), "changing profile cannot unlock market");
    snapshot.status = OwnershipStatus::ready;
    snapshot.player_vehicles = 0;
    check(!can_open_owned_cargo_market(snapshot), "missing vehicle list remains locked");
    snapshot.player_vehicles = reinterpret_cast<std::uintptr_t>(vehicles.data());
    snapshot.trailers.clear();
    check(!can_open_owned_cargo_market(snapshot), "selling final trailer removes eligibility");
    std::cout << checks << " market access checks passed\n";
}
