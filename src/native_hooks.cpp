#include "native_hooks.hpp"
#include "native_host.hpp"

namespace ets2::native {
namespace {

// A leaf SEH boundary solely around an atomic pointer operation. It contains
// no C++ objects requiring unwinding and never wraps a call into game code.
// Readability checks do not by themselves prevent a concurrent unmap.
bool exchange_if_equal(std::uintptr_t address, std::uintptr_t expected,
                       std::uintptr_t replacement, std::uintptr_t& observed) noexcept {
    __try {
        observed = reinterpret_cast<std::uintptr_t>(InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID volatile*>(address), reinterpret_cast<PVOID>(replacement),
            reinterpret_cast<PVOID>(expected)));
        return true;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                    ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}

bool make_writable(std::uintptr_t address, DWORD& previous) noexcept {
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region)) != sizeof(region)
        || region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const DWORD basic = region.Protect & 0xff;
    const bool executable = basic == PAGE_EXECUTE || basic == PAGE_EXECUTE_READ
        || basic == PAGE_EXECUTE_READWRITE || basic == PAGE_EXECUTE_WRITECOPY;
    const DWORD writable = (executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE)
        | (region.Protect & (PAGE_NOCACHE | PAGE_WRITECOMBINE));
    return VirtualProtect(reinterpret_cast<void*>(address), sizeof(void*), writable, &previous) != FALSE;
}

} // namespace

const char* hook_status_name(HookStatus status) noexcept {
    switch (status) {
    case HookStatus::installed: return "installed";
    case HookStatus::restored: return "restored";
    case HookStatus::not_installed: return "not_installed";
    case HookStatus::invalid_argument: return "invalid_argument";
    case HookStatus::busy: return "busy";
    case HookStatus::unreadable: return "unreadable";
    case HookStatus::unexpected_pointer: return "unexpected_pointer";
    case HookStatus::protection_change_failed: return "protection_change_failed";
    case HookStatus::pointer_access_failed: return "pointer_access_failed";
    case HookStatus::pointer_conflict: return "pointer_conflict";
    case HookStatus::protection_restore_failed: return "protection_restore_failed";
    }
    return "unknown";
}

bool VtableSlotPatch::restore_protection() noexcept {
    if (!protection_pending_) return true;
    DWORD ignored = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(slot_), sizeof(void*), protection_to_restore_, &ignored))
        return false;
    protection_pending_ = false;
    return true;
}

HookStatus VtableSlotPatch::install(std::uintptr_t slot, std::uintptr_t expected_original,
                                   std::uintptr_t replacement) noexcept {
    if (installed_ || protection_pending_) return HookStatus::busy;
    if (!slot || slot % alignof(void*) != 0 || !expected_original || !replacement
        || expected_original == replacement) return HookStatus::invalid_argument;
    std::uintptr_t current = 0;
    if (!safe_read(slot, current)) return HookStatus::unreadable;
    if (current != expected_original) return HookStatus::unexpected_pointer;
    DWORD previous = 0;
    if (!make_writable(slot, previous)) return HookStatus::protection_change_failed;
    slot_ = slot;
    original_ = expected_original;
    replacement_ = replacement;
    protection_to_restore_ = previous;
    protection_pending_ = true;
    std::uintptr_t observed = 0;
    const bool accessed = exchange_if_equal(slot_, original_, replacement_, observed);
    installed_ = accessed && observed == original_;
    const auto result = !accessed ? HookStatus::pointer_access_failed
        : installed_ ? HookStatus::installed : HookStatus::pointer_conflict;
    if (restore_protection()) return result;

    // A failed protection restore makes installation fail. Roll back only our
    // own replacement; an intervening writer always keeps its pointer value.
    if (installed_) {
        if (exchange_if_equal(slot_, replacement_, original_, observed)) installed_ = false;
        // If the exchange faults, keep installed_ true until later readback.
    }
    // One cleanup retry after rollback; any remaining debt is exposed to caller.
    (void)restore_protection();
    return HookStatus::protection_restore_failed;
}

HookStatus VtableSlotPatch::restore() noexcept {
    if (!installed_) {
        if (!protection_pending_) return HookStatus::not_installed;
        return restore_protection() ? HookStatus::restored : HookStatus::protection_restore_failed;
    }
    std::uintptr_t current = 0;
    if (!safe_read(slot_, current)) return HookStatus::unreadable;
    if (current != replacement_) {
        installed_ = false;
        if (!restore_protection()) return HookStatus::protection_restore_failed;
        return current == original_ ? HookStatus::restored : HookStatus::pointer_conflict;
    }
    DWORD previous = 0;
    if (!make_writable(slot_, previous)) return HookStatus::protection_change_failed;
    // If an earlier cleanup is pending, retain that protection target. Otherwise
    // preserve the protection observed immediately before this restore attempt.
    if (!protection_pending_) protection_to_restore_ = previous;
    protection_pending_ = true;
    std::uintptr_t observed = 0;
    const bool accessed = exchange_if_equal(slot_, replacement_, original_, observed);
    if (accessed) installed_ = false;
    if (!restore_protection()) return HookStatus::protection_restore_failed;
    if (!accessed) return HookStatus::pointer_access_failed;
    return observed == replacement_ || observed == original_ ? HookStatus::restored : HookStatus::pointer_conflict;
}

} // namespace ets2::native
