#include "../src/native_ownership.hpp"
#include "../src/native_host.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
using namespace ets2::cargo_filter::native;
int checks = 0;
void check(bool pass, const char* message) {
    ++checks;
    if (!pass) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
template<class T> void put(std::uintptr_t address, std::size_t offset, const T& value) {
    std::memcpy(reinterpret_cast<void*>(address + offset), &value, sizeof(value));
}
struct Object {
    std::vector<std::byte> bytes;
    explicit Object(std::size_t size) : bytes(size) { put(address(), 8, std::uint32_t{0x80000001}); }
    std::uintptr_t address() const { return reinterpret_cast<std::uintptr_t>(bytes.data()); }
};
void array(std::uintptr_t object, std::size_t offset, const std::vector<std::uintptr_t>& values) {
    put(object, offset + 8, reinterpret_cast<std::uintptr_t>(values.data()));
    put(object, offset + 16, static_cast<std::uint64_t>(values.size()));
    put(object, offset + 24, static_cast<std::uint64_t>(values.size()));
}
}

int main() {
    // A synthetic readable region stands in for the already build-gated module;
    // no installed game, live process attachment or engine call is used.
    auto* memory = VirtualAlloc(nullptr, economy_singleton_rva + sizeof(std::uintptr_t),
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    check(memory != nullptr, "synthetic module allocation");
    const auto module = reinterpret_cast<std::uintptr_t>(memory);
    check(read_ownership(module).status == OwnershipStatus::no_profile, "unloaded profile is ordinary state");

    Object economy(economy_size), player(player_size), vehicles(player_vehicles_size);
    Object a(trailer_size), b(trailer_size), definition_a(0xd0), definition_b(0xd0);
    std::vector<std::uintptr_t> my_vehicles{vehicles.address()};
    std::vector<std::uintptr_t> owned{a.address(), b.address()};
    put(module, economy_singleton_rva, economy.address());
    put(economy.address(), 0x18, player.address());
    array(player.address(), 0x70, my_vehicles);
    array(player.address(), 0x1a8, owned);
    put(vehicles.address(), 0x48, b.address());
    put(a.address(), 0x1a0, definition_a.address());
    put(b.address(), 0x1a0, definition_b.address());
    // Native base-38 token vectors, using the executable's digit alphabet.
    put(definition_a.address(), 0x48, std::uint64_t{0x72d25952}); // dryvan
    put(definition_b.address(), 0x48, std::uint64_t{0x2fba7492e9ed8b86}); // refrigerated
    put(a.address(), 0x1c0, std::uint8_t{1});
    std::string plate = "<color value=FFFFFFFF>34 TEST 01|turkey";
    put(a.address(), 0x78, reinterpret_cast<std::uintptr_t>(plate.data()));
    put(a.address(), 0x80, static_cast<std::uint32_t>(plate.size()));

    const auto snapshot = read_ownership(module);
    check(snapshot.status == OwnershipStatus::ready && snapshot.trailers.size() == 2,
          "enumerates all owned trailers, not only active trailer");
    check(snapshot.active_trailer == b.address() && snapshot.trailers[0].address == a.address(),
          "active trailer is identified separately from original ownership order");
    check(snapshot.trailers[0].definition == definition_a.address()
          && snapshot.trailers[1].definition == definition_b.address(),
          "per-trailer definitions are retained exactly without type inference");
    check(snapshot.trailers[0].label == "Kuru Yük - 34 TEST 01" && snapshot.trailers[1].label == "Frigorifik",
          "actual Turkish type names replace ordinals and plate markup is stripped");
    check(snapshot.trailers[0].body_type == "dryvan" && snapshot.trailers[1].body_type == "refrigerated",
          "native body tokens are decoded exactly including twelve-character names");
    check(snapshot.trailers[0].is_private && !snapshot.trailers[1].is_private
          && snapshot.trailers[0].session_id != snapshot.trailers[1].session_id,
          "privacy does not hide ownership and exact session identities differ");

    put(definition_a.address(), 0x48, std::uint64_t{0x815717e830ef4c37});
    check(read_ownership(module).trailers[0].type_name == "Perdeli",
          "token flag bit does not change the actual curtainside type");
    put(definition_a.address(), 0x48, std::uint64_t{0x390106bfa35f});
    check(read_ownership(module).trailers[0].type_name == "Yalıtımlı",
          "insulated remains distinct from refrigerated despite shared filter category");
    put(definition_a.address(), 0x48, std::uint64_t{0x65798c5acd54c810});
    check(read_ownership(module).trailers[0].type_name == "Flatbed / Konteyner",
          "underscore-bearing known token uses shipped display name");
    put(definition_a.address(), 0x48, std::uint64_t{0x184d85301d});
    const auto custom = read_ownership(module);
    check(custom.trailers[0].body_type == "mod_box" && custom.trailers[0].label == "Mod box - 34 TEST 01",
          "custom body type is represented without guessing a vanilla type");
    put(definition_a.address(), 0x48, std::uint64_t{0x2fba7492e9ed8b86});
    std::string plate_b = "<font>06 TEST 02</font>|turkey";
    put(b.address(), 0x78, reinterpret_cast<std::uintptr_t>(plate_b.data()));
    put(b.address(), 0x80, static_cast<std::uint32_t>(plate_b.size()));
    const auto duplicates = read_ownership(module);
    check(duplicates.trailers[0].label == "Frigorifik - 34 TEST 01"
          && duplicates.trailers[1].label == "Frigorifik - 06 TEST 02",
          "plates distinguish multiple trailers with the same real type");
    put(definition_a.address(), 0x48, std::uint64_t{38});
    check(read_ownership(module).trailers[0].body_type.empty(),
          "embedded token terminator cannot produce a misleading type");
    put(definition_a.address(), 0x48, std::uint64_t{0});
    check(read_ownership(module).trailers[0].label == "Dorse (tür bilinmiyor) - 34 TEST 01",
          "missing body type has explicit fallback and keeps identifying plate");
    put(definition_a.address(), 0x48, std::uint64_t{0x72d25952});
    put(definition_a.address(), 8, std::uint32_t{1});
    check(read_ownership(module).trailers[0].body_type.empty(),
          "stale trailer definition is not read as a valid type");
    put(definition_a.address(), 8, std::uint32_t{0x80000001});
    put(a.address(), 0x1a0, std::uintptr_t{1});
    check(read_ownership(module).trailers[0].type_name == "Dorse (tür bilinmiyor)",
          "unreadable definition keeps snapshot safe without invented type");

    put(a.address(), 0x1a0, std::uintptr_t{0});
    const auto missing_definition = read_ownership(module);
    check(missing_definition.status == OwnershipStatus::ready
          && missing_definition.trailers[0].definition == 0, "missing definition does not invent compatibility");
    put(a.address(), 8, std::uint32_t{1});
    check(read_ownership(module).status == OwnershipStatus::unavailable, "dead unit is rejected");
    put(a.address(), 8, std::uint32_t{0x80000001});

    put(player.address(), 0x1b8, std::uint64_t{10000000});
    check(read_ownership(module).status == OwnershipStatus::unavailable, "corrupt count cannot cause unbounded allocation");
    array(player.address(), 0x1a8, owned);
    put(player.address(), 0x1b0, std::uintptr_t{1});
    check(read_ownership(module).status == OwnershipStatus::unavailable, "unreadable native pointers fail safely");

    const std::vector<std::uintptr_t> empty;
    array(player.address(), 0x1a8, empty);
    const auto no_trailers = read_ownership(module);
    check(no_trailers.status == OwnershipStatus::ready && no_trailers.trailers.empty(),
          "empty ownership remains a valid snapshot");
    array(player.address(), 0x70, empty);
    check(read_ownership(module).status == OwnershipStatus::ready, "no vehicle category can be loaded yet");
    put(economy.address(), 0x18, std::uintptr_t{0});
    check(read_ownership(module).status == OwnershipStatus::no_profile, "profile unload drops all owned results");
    check(VirtualFree(memory, 0, MEM_RELEASE) != FALSE, "synthetic module cleanup");
    std::cout << "Passed " << checks << " native ownership checks.\n";
}
