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
struct Pages {
    void* data = nullptr;
    ~Pages() { if (data) VirtualFree(data, 0, MEM_RELEASE); }
};
} // namespace

int main() {
    using namespace ets2::native;
    check(matches_expected_build(expected_build), "exact known identity admitted");
    auto identity = expected_build;
    identity.sha256[31] ^= 1;
    check(!matches_expected_build(identity), "same-size modified executable rejected");
    identity = expected_build; ++identity.file_size;
    check(!matches_expected_build(identity), "different file size rejected");
    identity = expected_build; identity.machine = IMAGE_FILE_MACHINE_I386;
    check(!matches_expected_build(identity), "x86 executable rejected");
    identity = expected_build; identity.optional_magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    check(!matches_expected_build(identity), "wrong PE optional header rejected");
    identity = expected_build; ++identity.image_size;
    check(!matches_expected_build(identity), "wrong image bounds rejected");

    std::uint64_t source = 0x123456789abcdef0ULL, result = 0;
    check(safe_read(reinterpret_cast<std::uintptr_t>(&source), result) && result == source,
          "readable local value copied");
    result = 42;
    check(!safe_read(std::uintptr_t{0}, result) && result == 42,
          "null read rejected and typed destination unchanged");
    check(!safe_read_bytes(std::numeric_limits<std::uintptr_t>::max() - 1, &result, 8),
          "overflowing address range rejected");
    check(!safe_read_bytes(reinterpret_cast<std::uintptr_t>(&source), nullptr, 8),
          "null destination rejected");
    check(!safe_read_bytes(reinterpret_cast<std::uintptr_t>(&source), &result, 1024 * 1024 + 1),
          "oversized read rejected before memory access");

    SYSTEM_INFO info{}; GetSystemInfo(&info);
    const auto page_size = static_cast<std::size_t>(info.dwPageSize);
    Pages pages{VirtualAlloc(nullptr, page_size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)};
    check(pages.data != nullptr, "allocate isolated test pages");
    auto* first = static_cast<unsigned char*>(pages.data);
    first[page_size - 1] = 0x7a;
    first[page_size] = 0x3b;
    const auto address = reinterpret_cast<std::uintptr_t>(first + page_size - 1);
    std::uint16_t across = 0;
    check(safe_read(address, across) && across == 0x3b7a, "read across adjacent readable pages");
    DWORD previous = 0;
    check(VirtualProtect(first + page_size, page_size, PAGE_NOACCESS, &previous) != FALSE,
          "protect second test page");
    across = 71;
    check(!safe_read(address, across) && across == 71,
          "range crossing inaccessible page rejected without changing typed output");
    check(VirtualProtect(first + page_size, page_size, PAGE_READWRITE | PAGE_GUARD, &previous) != FALSE,
          "set guard on isolated test page");
    check(!safe_read(reinterpret_cast<std::uintptr_t>(first + page_size), across),
          "guard page is never touched");

    HostContext context{};
    context.module_base = 0x140000000;
    context.module_size = 0x1000;
    check(context.address(0x20, 8) == context.module_base + 0x20, "bounded RVA resolved");
    check(context.address(0x1000) == 0 && context.address(0xfff, 2) == 0,
          "out-of-image RVA and crossing read rejected");
    context.module_base = std::numeric_limits<std::uintptr_t>::max() - 1;
    check(context.address(4) == 0, "overflowing image address rejected");
    std::cout << "Passed " << checks << " native-host checks.\n";
}
