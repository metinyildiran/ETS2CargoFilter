#include "native_hooks.hpp"
#include "native_host.hpp"
#include <cstdlib>
#include <iostream>

namespace {
using namespace ets2::native;
int checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
struct Page {
    void* pointer = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    ~Page() { if (pointer) VirtualFree(pointer, 0, MEM_RELEASE); }
};
DWORD protection(void* pointer) {
    MEMORY_BASIC_INFORMATION region{};
    check(VirtualQuery(pointer, &region, sizeof(region)) == sizeof(region), "query local test page");
    return region.Protect;
}
void replace_externally(std::uintptr_t* slot, std::uintptr_t value) {
    DWORD previous = 0, ignored = 0;
    check(VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &previous) != FALSE, "external writer obtains writable test page");
    InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(slot), reinterpret_cast<void*>(value));
    check(VirtualProtect(slot, sizeof(*slot), previous, &ignored) != FALSE, "external writer restores test page protection");
}
} // namespace

int main() {
    Page page;
    check(page.pointer != nullptr, "allocate local test page");
    auto* slot = static_cast<std::uintptr_t*>(page.pointer);
    constexpr std::uintptr_t original = 0x140012340, replacement = 0x180012340, external = 0x180056780;
    *slot = original;
    DWORD previous = 0;
    check(VirtualProtect(page.pointer, 4096, PAGE_READONLY, &previous) != FALSE, "make test vtable read-only");
    const auto address = reinterpret_cast<std::uintptr_t>(slot);

    VtableSlotPatch patch;
    check(patch.restore() == HookStatus::not_installed, "restore empty patch is a no-op");
    check(patch.install(address + 1, original, replacement) == HookStatus::invalid_argument, "unaligned slot rejected");
    check(patch.install(address, original, original) == HookStatus::invalid_argument, "same-pointer patch rejected");
    check(patch.install(0, original, replacement) == HookStatus::invalid_argument, "null slot rejected");
    Page inaccessible;
    check(inaccessible.pointer != nullptr, "allocate isolated inaccessible test page");
    DWORD inaccessible_previous = 0;
    check(VirtualProtect(inaccessible.pointer, 4096, PAGE_NOACCESS, &inaccessible_previous) != FALSE,
          "deny access to isolated test page");
    check(patch.install(reinterpret_cast<std::uintptr_t>(inaccessible.pointer), original, replacement)
              == HookStatus::unreadable, "inaccessible slot rejected");
    check(protection(inaccessible.pointer) == PAGE_NOACCESS, "rejection never lifts inaccessible protection");
    check(patch.install(address, external, replacement) == HookStatus::unexpected_pointer,
          "wrong expected pointer rejected");
    check(*slot == original && protection(slot) == PAGE_READONLY, "mismatch preserves pointer and protection");
    check(patch.install(address, original, replacement) == HookStatus::installed, "expected pointer installed atomically");
    check(*slot == replacement && patch.installed() && !patch.protection_pending(), "installed state reflects completed patch");
    check(protection(slot) == PAGE_READONLY, "install restores read-only protection");
    check(patch.install(address, replacement, external) == HookStatus::busy, "active record cannot be overwritten");
    check(patch.restore() == HookStatus::restored && *slot == original, "restore replaces our pointer only");
    check(!patch.installed() && !patch.protection_pending() && protection(slot) == PAGE_READONLY,
          "restore completes ownership and protection cleanup");
    check(patch.restore() == HookStatus::not_installed, "double restore is harmless");

    check(patch.install(address, original, replacement) == HookStatus::installed, "record reusable after restore");
    replace_externally(slot, external);
    check(patch.restore() == HookStatus::pointer_conflict, "external replacement reported");
    check(*slot == external && !patch.installed(), "restore never overwrites external pointer");
    check(protection(slot) == PAGE_READONLY, "conflict leaves protection unchanged");
    replace_externally(slot, original);
    check(patch.install(address, original, replacement) == HookStatus::installed, "install after external conflict");
    replace_externally(slot, original);
    check(patch.restore() == HookStatus::restored && !patch.installed(), "already-original slot recognized without overwrite");
    check(protection(slot) == PAGE_READONLY, "already-restored slot retains page protection");
    std::cout << "Passed " << checks << " native-hook checks.\n";
}
