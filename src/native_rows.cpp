#include "native_rows.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace ets2::native {
namespace {

struct alignas(8) NativeRow {
    std::array<std::byte, 0xc8> bytes{};
};
static_assert(sizeof(NativeRow) == 0xc8);
static_assert(alignof(NativeRow) == 8);

template<class T>
T read(std::uintptr_t address) noexcept {
    T value{};
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
}
template<class T>
void write(std::uintptr_t address, T value) noexcept {
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
}
template<class F>
F entry(const HostContext& host, std::size_t rva) noexcept {
    return reinterpret_cast<F>(host.address(rva));
}

bool live_unit(std::uintptr_t object) noexcept {
    return object && read<std::int32_t>(object + 8) < 0;
}

AppendRowResult append_unsafe(const HostContext& host, std::uintptr_t task,
                            std::uintptr_t company, std::uintptr_t offer) {
    const auto city = entry<std::uintptr_t (*)(std::uintptr_t)>(host, 0x7d1150);
    const auto token_city = entry<std::uintptr_t (*)(std::uintptr_t)>(host, 0x847ec0);
    const auto destination = entry<std::uintptr_t (*)(std::uintptr_t)>(host, 0x7d1300);
    const auto fill = entry<void (*)(std::uintptr_t, std::uintptr_t,
                                  std::uintptr_t, std::uintptr_t)>(host, 0x1072620);
    const auto append = entry<std::uintptr_t (*)(std::uintptr_t, std::uintptr_t)>(host, 0x10745c0);
    const auto destroy = entry<void (*)(std::uintptr_t)>(host, 0x1073e50);
    if (!city || !token_city || !destination || !fill || !append || !destroy
        || !host.address(0x21f3a10) || !host.address(0x251d6e8, sizeof(float))) {
        return AppendRowResult::failed;
    }
    if (read<std::uint8_t>(task + 0x24)) return AppendRowResult::skipped;
    if (!live_unit(company) || !live_unit(offer)) return AppendRowResult::failed;
    const auto source_constraint = read<std::uintptr_t>(task + 0x350);
    const auto source_city = city(company);
    if (source_constraint) {
        if (source_constraint != company) return AppendRowResult::skipped;
    } else {
        if (!read<std::uint8_t>(company + 0xf8)
            || read<std::int32_t>(company + 0x70) == 2) return AppendRowResult::skipped;
        // Native resolution uses a local copy of the task's source token.
        const auto source_token = read<std::uint64_t>(task + 0x158);
        const auto requested_city = token_city(reinterpret_cast<std::uintptr_t>(&source_token));
        if (requested_city && requested_city != source_city) {
            if (!source_city) return AppendRowResult::skipped;
            const auto requested_id = read<std::uint64_t>(requested_city + 0x90);
            const auto actual_id = read<std::uint64_t>(source_city + 0x90);
            constexpr std::uint64_t token_mask = 0x7fffffffffffffffULL;
            if (!(requested_id & token_mask) || !(actual_id & token_mask)
                || requested_id != actual_id) return AppendRowResult::skipped;
        }
    }
    const auto cargo = read<std::uintptr_t>(offer + 0x58);
    if (!live_unit(cargo) || read<std::uint8_t>(offer + 0xc8)
        || read<std::uint32_t>(task + 0x160) >= read<std::uint32_t>(offer + 0x48)) {
        return AppendRowResult::skipped;
    }
    // Invoke the task's original search/type predicate after ordinary validity
    // checks, in the same order used by Cargo Market's native enumeration.
    const auto predicate = reinterpret_cast<bool (*)(std::uintptr_t, std::uintptr_t, std::uintptr_t)>(
        read<std::uintptr_t>(read<std::uintptr_t>(task) + 0x20));
    if (!predicate || !predicate(task, company, offer)) return AppendRowResult::skipped;
    const auto target = destination(offer + 0x18);
    // Vanilla checks target conditionally but later dereferences it. Reject a
    // null target explicitly rather than reproduce the native null dereference.
    if (!target || read<std::int32_t>(target + 0x70) == 2) return AppendRowResult::skipped;
    const auto target_city = city(target);
    const auto descriptor = read<std::uintptr_t>(target + 0x68);
    if (!source_city || !target_city || !descriptor) return AppendRowResult::failed;

    NativeRow row{};
    const auto address = reinterpret_cast<std::uintptr_t>(row.bytes.data());
    for (const auto offset : {std::size_t{0x28}, std::size_t{0x48}, std::size_t{0x68}}) {
        write(address + offset, host.address(0x21f3a10));
        write(address + offset + 8, address + offset + 0x18);
        write<std::uint32_t>(address + offset + 0x14, 8);
    }
    // From here the native destructor balances each successfully retained field.
    // All other reference fields and flags start zero, as in the native row.
    std::uintptr_t inserted = 0;
    __try {
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(company + 8));
        write(address, company);
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(offer + 8));
        write(address + 8, offer);
        const auto distance = static_cast<float>(read<std::uint16_t>(offer + 0x50));
        float mass = static_cast<float>(read<std::uint32_t>(offer + 0xcc)) * read<float>(cargo + 0x9c);
        const auto accessory = read<std::uintptr_t>(offer + 0x90);
        if (mass < read<float>(host.address(0x251d6e8)) && accessory) mass = read<float>(accessory + 0x34);
        write(address + 0x90, distance);
        write(address + 0x94, mass);
        fill(task, address, source_city, target_city);
        // The fixed native string's reserve method always returns eight bytes.
        // Native enumeration copies min(descriptor length, 7) plus its terminator.
        const auto text_length = std::min(read<std::uint32_t>(descriptor + 0x40), std::uint32_t{7});
        const auto text_data = read<std::uintptr_t>(descriptor + 0x38);
        if (text_length) {
            std::memcpy(reinterpret_cast<void*>(address + 0x80),
                        reinterpret_cast<const void*>(text_data), text_length);
        }
        write<std::uint32_t>(address + 0x78, text_length);
        write<std::uint8_t>(address + 0x80 + text_length, 0);
        inserted = append(task + 0x168, address);
    } __finally {
        destroy(address);
    }
    return inserted ? AppendRowResult::appended : AppendRowResult::failed;
}

} // namespace

bool NativeRows::prepare_city_keys(std::uintptr_t task) const noexcept {
    const auto prepare = entry<void (*)(std::uintptr_t)>(host_, 0x1072810);
    if (!task || !prepare) return false;
    __try {
        if (read<std::uint8_t>(task + 0x24)) return false;
        prepare(task);
        return !read<std::uint8_t>(task + 0x24);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

AppendRowResult NativeRows::append(std::uintptr_t task,
    std::uintptr_t original_company, std::uintptr_t native_offer) const noexcept {
    if (!task || !original_company || !native_offer) return AppendRowResult::failed;
    __try {
        return append_unsafe(host_, task, original_company, native_offer);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return AppendRowResult::failed;
    }
}

bool NativeRows::calculate_pay(std::uintptr_t task) const noexcept {
    const auto calculate = entry<void (*)(std::uintptr_t)>(host_, 0x1073990);
    if (!task || !calculate) return false;
    __try {
        if (read<std::uint8_t>(task + 0x24)) return false;
        calculate(task);
        return !read<std::uint8_t>(task + 0x24);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace ets2::native
