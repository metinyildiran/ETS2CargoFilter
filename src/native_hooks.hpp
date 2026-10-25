#pragma once

#include <cstdint>

namespace ets2::native {

enum class HookStatus {
    installed,
    restored,
    not_installed,
    invalid_argument,
    busy,
    unreadable,
    unexpected_pointer,
    protection_change_failed,
    pointer_access_failed,
    pointer_conflict,
    protection_restore_failed,
};

[[nodiscard]] const char* hook_status_name(HookStatus status) noexcept;

// One aligned, pointer-sized vtable slot. The caller owns serialization, the
// target mapping's lifetime and exact-build/code validation. This class never
// changes instruction bytes, discovers targets, or overwrites another hook.
// Explicitly restore and check its result before unloading replacement code.
// Destruction intentionally does not silently attempt fallible cleanup.
class VtableSlotPatch {
public:
    VtableSlotPatch() = default;
    VtableSlotPatch(const VtableSlotPatch&) = delete;
    VtableSlotPatch& operator=(const VtableSlotPatch&) = delete;
    VtableSlotPatch(VtableSlotPatch&&) = delete;
    VtableSlotPatch& operator=(VtableSlotPatch&&) = delete;

    [[nodiscard]] HookStatus install(std::uintptr_t slot, std::uintptr_t expected_original,
                                     std::uintptr_t replacement) noexcept;
    [[nodiscard]] HookStatus restore() noexcept;

    // On an access fault, installed stays conservatively true until readback
    // or restore can establish that the slot no longer refers to replacement.
    [[nodiscard]] bool installed() const noexcept { return installed_; }
    [[nodiscard]] bool protection_pending() const noexcept { return protection_pending_; }
    [[nodiscard]] std::uintptr_t original() const noexcept { return original_; }
    [[nodiscard]] std::uintptr_t slot() const noexcept { return slot_; }

private:
    [[nodiscard]] bool restore_protection() noexcept;
    std::uintptr_t slot_ = 0;
    std::uintptr_t original_ = 0;
    std::uintptr_t replacement_ = 0;
    std::uint32_t protection_to_restore_ = 0;
    bool installed_ = false;
    bool protection_pending_ = false;
};

} // namespace ets2::native
