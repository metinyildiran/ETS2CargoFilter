#include "native_ownership.hpp"
#include "native_host.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_set>

namespace ets2::cargo_filter::native {
namespace {
using ets2::native::safe_read;
using ets2::native::safe_read_bytes;

struct NativeArray {
    std::uintptr_t vtable = 0;
    std::uintptr_t data = 0;
    std::uint64_t count = 0;
    std::uint64_t capacity = 0;
    std::uintptr_t allocator = 0;
    bool operator==(const NativeArray&) const = default;
};
static_assert(sizeof(NativeArray) == 0x28);

constexpr std::uint64_t max_trailers = 65536;

bool add(std::uintptr_t base, std::size_t offset, std::uintptr_t& result) noexcept {
    if (!base || offset > std::numeric_limits<std::uintptr_t>::max() - base) return false;
    result = base + offset;
    return true;
}

template<class T>
bool field(std::uintptr_t object, std::size_t offset, T& result) noexcept {
    std::uintptr_t address = 0;
    return add(object, offset, address) && safe_read(address, result);
}

bool live_unit(std::uintptr_t object, std::size_t bytes) noexcept {
    if (!object || object % alignof(void*) != 0 || bytes > economy_size) return false;
    std::array<std::byte, economy_size> storage{};
    if (!safe_read_bytes(object, storage.data(), bytes)) return false;
    std::int32_t flags = 0;
    std::memcpy(&flags, storage.data() + 8, sizeof(flags));
    // The game's observed link checks require the sign bit to be set.
    return flags < 0;
}

bool pointer_array(std::uintptr_t object, std::size_t offset, NativeArray& array,
                   std::vector<std::uintptr_t>& entries, std::uint64_t limit) {
    if (!field(object, offset, array) || array.count > limit || array.count > array.capacity
        || (array.count && (!array.data || array.data % alignof(void*) != 0))) return false;
    entries.resize(static_cast<std::size_t>(array.count));
    return safe_read_bytes(array.data, entries.data(), entries.size() * sizeof(std::uintptr_t));
}

std::string read_plate(std::uintptr_t trailer) {
    std::uintptr_t data = 0;
    std::uint32_t length = 0;
    // string field +0x70: vtable, data pointer, length, capacity, inline storage.
    if (!field(trailer, 0x78, data) || !field(trailer, 0x80, length) || length > 512) return {};
    if (length == 0) return {};
    std::string result(static_cast<std::size_t>(length), '\0');
    if (!safe_read_bytes(data, result.data(), result.size())) return {};
    const auto nul = result.find('\0');
    if (nul != std::string::npos) result.resize(nul);
    return result;
}

std::string plate_label(const std::string& raw) {
    std::string result;
    bool in_tag = false;
    for (const unsigned char c : raw) {
        if (c == '|') break; // SCS's stored country suffix is not the plate text.
        if (c == '<') { in_tag = true; continue; }
        if (c == '>') { in_tag = false; continue; }
        if (!in_tag && c >= 0x20 && c != 0x7f) result.push_back(static_cast<char>(c));
    }
    const auto first = result.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    return result.substr(first, result.find_last_not_of(' ') - first + 1);
}

std::string read_body_type(std::uintptr_t definition) {
    // trailer_def descriptor 0x141e223c0: size 0xd0, body_type token +0x48.
    // 0x1400f1110 converts tokens with this alphabet (0x141df2410), least
    // significant digit first, masking the top flag bit. No engine calls/cache.
    if (!live_unit(definition, 0xd0)) return {};
    std::uint64_t token = 0;
    if (!field(definition, 0x48, token)) return {};
    token &= 0x7fffffffffffffffULL;
    constexpr char alphabet[] = "\0" "0123456789abcdefghijklmnopqrstuvwxyz_";
    std::string result;
    while (token && result.size() < 12) {
        const auto digit = token % 38;
        token /= 38;
        // An embedded terminator with nonzero trailing digits is corrupt.
        if (digit == 0) return {};
        result.push_back(alphabet[digit]);
    }
    return token ? std::string{} : result;
}

std::string body_type_name(const std::string& token) {
    // Display names from shipped locale/tr_tr/localization.sui, section
    // "Trailer body type names". Keep distinct body types (e.g. insulated and
    // refrigerated) rather than collapsing them to the cargo-filter category.
    struct Name { std::string_view token, label; };
    static constexpr Name names[] = {
        {"chemtank", "Kimyasal Yük Tankeri"}, {"container", "Konteyner Taşıyıcısı"},
        {"curtainside", "Perdeli"}, {"dryvan", "Kuru Yük"}, {"dumper", "Damper"},
        {"flatbed", "Flatbed"}, {"flatbed_brck", "Tuğla"},
        {"flatbed_cont", "Flatbed / Konteyner"}, {"foodtank", "Gıda Tankeri"},
        {"fueltank", "Akaryakıt Tankı"}, {"gastank", "Gaz Tankı"},
        {"insulated", "Yalıtımlı"}, {"livestock", "Canlı Çiftlik Hayvanı"},
        {"log", "Tomruk"}, {"lowbed", "Low Bed"}, {"lowboy", "Low Loader"},
        {"refrigerated", "Frigorifik"}, {"silo", "Silo"},
        {"inloader", "İç Yüklemeli"}
    };
    for (const auto& name : names) if (name.token == token) return std::string(name.label);
    if (token.empty()) return "Dorse (tür bilinmiyor)";
    // Custom body types retain their actual token, with only display casing and
    // separators changed. Never infer a vanilla type from a mod filename.
    auto result = token;
    std::replace(result.begin(), result.end(), '_', ' ');
    if (result.front() >= 'a' && result.front() <= 'z') result.front() -= 'a' - 'A';
    return result;
}

std::string session_id(std::uintptr_t player, std::uintptr_t trailer) {
    std::ostringstream out;
    out << std::hex << player << ':' << trailer;
    return out.str();
}

OwnershipSnapshot failure(OwnershipSnapshot result, OwnershipStatus status, const char* message) {
    result.status = status;
    result.error = message;
    result.trailers.clear();
    return result;
}
} // namespace

OwnershipSnapshot read_ownership(std::uintptr_t module_base) {
    OwnershipSnapshot result;
    if (!field(module_base, economy_singleton_rva, result.economy))
        return failure(std::move(result), OwnershipStatus::unavailable, "Economy pointer is unreadable");
    if (!result.economy)
        return failure(std::move(result), OwnershipStatus::no_profile, "No economy is loaded");
    if (!live_unit(result.economy, economy_size) || !field(result.economy, 0x18, result.player))
        return failure(std::move(result), OwnershipStatus::unavailable, "Economy is not a readable live unit");
    if (!result.player)
        return failure(std::move(result), OwnershipStatus::no_profile, "No player is loaded");
    if (!live_unit(result.player, player_size))
        return failure(std::move(result), OwnershipStatus::unavailable, "Player is not a readable live unit");

    NativeArray vehicles_array{}, trailers_array{};
    std::vector<std::uintptr_t> vehicles, trailers;
    if (!pointer_array(result.player, 0x70, vehicles_array, vehicles, 64)
        || !pointer_array(result.player, 0x1a8, trailers_array, trailers, max_trailers))
        return failure(std::move(result), OwnershipStatus::unavailable, "Invalid player ownership arrays");
    if (!vehicles.empty()) {
        result.player_vehicles = vehicles.front();
        if (!live_unit(result.player_vehicles, player_vehicles_size)
            || !field(result.player_vehicles, 0x48, result.active_trailer))
            return failure(std::move(result), OwnershipStatus::unavailable, "Invalid current player vehicles");
    }

    std::unordered_set<std::uintptr_t> seen;
    result.trailers.reserve(trailers.size());
    for (std::size_t i = 0; i < trailers.size(); ++i) {
        const auto address = trailers[i];
        if (!address || !seen.insert(address).second) continue;
        if (!live_unit(address, trailer_size))
            return failure(std::move(result), OwnershipStatus::unavailable, "An owned trailer is no longer live");
        OwnedTrailer trailer;
        trailer.address = address;
        std::uint8_t private_flag = 0;
        if (!field(address, 0x1a0, trailer.definition) || !field(address, 0x1c0, private_flag))
            return failure(std::move(result), OwnershipStatus::unavailable, "An owned trailer could not be read");
        trailer.is_private = private_flag != 0;
        // A missing definition remains visible as owned but cannot provide offers.
        trailer.session_id = session_id(result.player, address);
        trailer.raw_license_plate = read_plate(address);
        trailer.body_type = read_body_type(trailer.definition);
        trailer.type_name = body_type_name(trailer.body_type);
        trailer.label = trailer.type_name;
        const auto plate = plate_label(trailer.raw_license_plate);
        if (!plate.empty()) trailer.label += " - " + plate;
        result.trailers.push_back(std::move(trailer));
    }

    std::uintptr_t economy_now = 0, player_now = 0, active_now = 0;
    NativeArray vehicles_now{}, trailers_now{};
    std::vector<std::uintptr_t> vehicle_entries_now, trailer_entries_now;
    if (!field(module_base, economy_singleton_rva, economy_now) || economy_now != result.economy
        || !field(result.economy, 0x18, player_now) || player_now != result.player
        || !pointer_array(result.player, 0x70, vehicles_now, vehicle_entries_now, 64)
        || !pointer_array(result.player, 0x1a8, trailers_now, trailer_entries_now, max_trailers)
        || vehicles_now != vehicles_array || trailers_now != trailers_array
        || vehicle_entries_now != vehicles || trailer_entries_now != trailers
        || (result.player_vehicles && (!field(result.player_vehicles, 0x48, active_now)
            || active_now != result.active_trailer)))
        return failure(std::move(result), OwnershipStatus::changed, "Profile or ownership changed during snapshot");

    result.status = OwnershipStatus::ready;
    return result;
}
} // namespace ets2::cargo_filter::native
