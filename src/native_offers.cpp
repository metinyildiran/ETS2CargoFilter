#include "native_offers.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace ets2::native {
namespace {
using namespace cargo_filter::native;

template<std::size_t N> struct alignas(8) Shadow {
    std::array<std::byte, N> bytes{};
    std::uintptr_t address() noexcept { return reinterpret_cast<std::uintptr_t>(bytes.data()); }
    bool copy(std::uintptr_t source) noexcept { return safe_read_bytes(source, bytes.data(), N); }
    template<class T> void set(std::size_t offset, T value) noexcept {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }
    template<class T> T get(std::size_t offset) const noexcept {
        T value{};
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
};

// 14084fb50 reads [truck+0x38] and passes that array object to 140666db0.
// The latter iterates data/count at +8/+0x10 and calls 140904280 on each
// accessory. A live accessory+0x38 cache makes 140904280 return immediately;
// a cache miss otherwise lowercases the real accessory's path and writes its
// cache. Require every entry warm before invoking any native generation code.
bool preflight_readonly(std::uintptr_t truck) noexcept {
    const auto read_field = []<class T>(std::uintptr_t object, std::size_t offset, T& value) noexcept {
        return object >= 0x10000 && offset <= std::numeric_limits<std::uintptr_t>::max() - object
            && safe_read(object + offset, value);
    };
    const auto live = [&read_field](std::uintptr_t object) noexcept {
        std::int32_t flags = 0;
        return object % alignof(void*) == 0 && read_field(object, 8, flags) && flags < 0;
    };
    if (!live(truck)) return false;
    std::uintptr_t accessories = 0, data = 0;
    std::uint64_t count = 0, capacity = 0;
    if (!read_field(truck, 0x38, accessories) || accessories % alignof(void*) != 0
        || !read_field(accessories, 8, data) || !read_field(accessories, 0x10, count)
        || !read_field(accessories, 0x18, capacity) || count > capacity || count > 4096
        || (count && (!data || data % alignof(void*) != 0))) return false;
    for (std::uint64_t index = 0; index < count; ++index) {
        std::uintptr_t accessory = 0, cached_definition = 0;
        if (!read_field(data, static_cast<std::size_t>(index * sizeof(std::uintptr_t)), accessory)
            || !live(accessory) || !read_field(accessory, 0x38, cached_definition)
            || !live(cached_definition)) return false;
    }
    return true;
}

bool call_city(const HostContext& host, std::uintptr_t company) noexcept {
    __try {
        return reinterpret_cast<std::uintptr_t (*)(std::uintptr_t)>(host.address(0x7d1150))(company) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool call_generate(const HostContext& host, std::uintptr_t company, std::uintptr_t economy) noexcept {
    __try {
        reinterpret_cast<void (*)(std::uintptr_t, std::uintptr_t)>(host.address(0x7cfba0))(company, economy);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool retain_allocator(std::uintptr_t allocator) noexcept {
    if (!allocator) return false;
    __try {
        InterlockedExchangeAdd64(reinterpret_cast<volatile LONG64*>(allocator + 0x58), 0x100000000LL);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool destroy_array(const HostContext& host, std::uintptr_t array) noexcept {
    __try {
        reinterpret_cast<void (*)(std::uintptr_t)>(host.address(0x115140))(array);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
struct CompanyOwner {
    Shadow<company_size> object;
    bool initialized = false;
};
}

struct NativeOfferBatch::State {
    HostContext host;
    std::vector<std::unique_ptr<CompanyOwner>> companies;
    std::vector<GeneratedOffer> rows;
    std::string error;
    explicit State(const HostContext& context) : host(context) {}
    ~State() {
        for (auto& company : companies) {
            if (company->initialized && !destroy_array(host, company->object.address() + 0xa0)) {
                if (host.log) host.log(LogLevel::error, "Transient cargo array cleanup failed.");
            }
        }
    }
};

NativeOfferBatch::NativeOfferBatch(const HostContext& host) : state_(std::make_unique<State>(host)) {}
NativeOfferBatch::~NativeOfferBatch() = default;
const std::vector<GeneratedOffer>& NativeOfferBatch::offers() const noexcept { return state_->rows; }
const std::string& NativeOfferBatch::error() const noexcept { return state_->error; }

bool NativeOfferBatch::generate(const OwnershipSnapshot& snapshot, std::uintptr_t trailer,
                              std::uintptr_t source_constraint) {
    auto& state = *state_;
    const auto fail = [&state](const char* reason) { state.error = reason; return false; };
    if (GetCurrentThreadId() != state.host.main_thread_id)
        return fail("Offer generation requested outside the game main thread.");
    if (!state.companies.empty() || !state.rows.empty()) return fail("Offer batch cannot be regenerated.");
    if (snapshot.status != OwnershipStatus::ready || !trailer
        || std::none_of(snapshot.trailers.begin(), snapshot.trailers.end(),
                        [trailer](const auto& owned) { return owned.address == trailer; }))
        return fail("Selected trailer is not in the current ownership snapshot.");

    Shadow<economy_size> economy;
    Shadow<player_size> player;
    Shadow<player_vehicles_size> vehicles;
    if (!economy.copy(snapshot.economy) || !player.copy(snapshot.player)
        || !vehicles.copy(snapshot.player_vehicles)) return fail("Could not copy generator context.");
    std::uintptr_t singleton = 0;
    if (!safe_read(state.host.address(economy_singleton_rva), singleton) || singleton != snapshot.economy)
        return fail("Profile changed before offer generation.");
    if (!preflight_readonly(vehicles.get<std::uintptr_t>(0x10)))
        return fail("Truck accessory caches are not fully initialized for read-only generation.");

    // Both regular and fallback generators resolve the trailer through these
    // different paths. Only pointers inside our private copies are redirected.
    auto vehicle_pointer = vehicles.address();
    vehicles.set(0x48, trailer);
    player.set(0x78, reinterpret_cast<std::uintptr_t>(&vehicle_pointer));
    player.set<std::uint64_t>(0x80, 1);
    player.set<std::uint64_t>(0x88, 1);
    player.set<std::uint32_t>(0x98, 0);
    player.set(0xa0, vehicle_pointer);
    economy.set(0x18, player.address());

    const auto company_data = economy.get<std::uintptr_t>(0x28);
    const auto company_count = economy.get<std::uint64_t>(0x30);
    if (!company_data || company_count > 20000) return fail("Unexpected company collection.");
    std::vector<std::uintptr_t> sources(static_cast<std::size_t>(company_count));
    if (!safe_read_bytes(company_data, sources.data(), sources.size() * sizeof(std::uintptr_t)))
        return fail("Could not read company collection.");

    // City lookup lazily caches retained city references. Resolve on the actual
    // company before copying so a shallow clone never owns an unbalanced cache.
    // This uses the engine's metadata cache; world offers and trailer stay intact.
    for (const auto company : sources) {
        if (!company || !call_city(state.host, company)) return fail("Could not resolve company city.");
    }
    for (const auto company : sources) {
        if (source_constraint && company != source_constraint) continue;
        auto owner = std::make_unique<CompanyOwner>();
        if (!owner->object.copy(company)) return fail("Could not copy company context.");
        auto& copy = owner->object;
        if (!source_constraint && (!copy.get<std::uint8_t>(0xf8) || copy.get<std::int32_t>(0x70) == 2)) continue;
        // The native generator indexes up to ten existing seeds; it does not
        // check before every branch. Never synthesize or refill world seeds.
        if (copy.get<std::uint64_t>(0x50) < 10 || !copy.get<std::uintptr_t>(0x48))
            return fail("Company has fewer than ten initialized cargo seeds.");
        const auto allocator = copy.get<std::uintptr_t>(0xc0);
        if (copy.get<std::uintptr_t>(0xa0) != state.host.address(0x21d88d8))
            return fail("Unexpected cargo owner array type.");
        copy.set<std::uintptr_t>(0xa8, 0);
        copy.set<std::uint64_t>(0xb0, 0);
        copy.set<std::uint64_t>(0xb8, 0);
        if (!retain_allocator(allocator)) return fail("Could not retain cargo allocator.");
        owner->initialized = true;
        auto* stored = owner.get();
        state.companies.push_back(std::move(owner));
        if (!call_generate(state.host, stored->object.address(), economy.address()))
            return fail("Native cargo generation raised an exception.");
        const auto count = stored->object.get<std::uint64_t>(0xb0);
        const auto data = stored->object.get<std::uintptr_t>(0xa8);
        if (count > 10 || (count && !data)) return fail("Unexpected generated cargo collection.");
        for (std::uint64_t index = 0; index < count; ++index) {
            std::uintptr_t offer = 0;
            if (!safe_read(data + index * 8, offer) || !offer) return fail("Invalid generated cargo unit.");
            state.rows.push_back({company, offer, trailer});
        }
    }
    return true;
}

} // namespace ets2::native
