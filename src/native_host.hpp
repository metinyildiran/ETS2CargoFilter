#pragma once

#ifndef _WIN32
#error The native ETS2 adapter requires Windows.
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <type_traits>

namespace ets2::native {

enum class LogLevel { info, warning, error };

struct BuildIdentity {
    std::uint64_t file_size = 0;
    std::array<std::uint8_t, 32> sha256{};
    std::uint16_t machine = 0;
    std::uint16_t optional_magic = 0;
    std::uint32_t image_size = 0;
};

inline constexpr BuildIdentity expected_build{
    53308304,
    {0xb7,0x0c,0x23,0x57,0xcc,0x56,0xeb,0x58,0x34,0xee,0x47,0xfe,0xa3,0xc6,0x0b,0x2f,
     0x2d,0x45,0x91,0x38,0x3f,0xf2,0xac,0x47,0x9c,0xfd,0x3c,0xd2,0x54,0x30,0x1b,0x8c},
    IMAGE_FILE_MACHINE_AMD64, IMAGE_NT_OPTIONAL_HDR64_MAGIC, 60338176
};

[[nodiscard]] inline bool matches_expected_build(const BuildIdentity& identity) noexcept {
    return identity.file_size == expected_build.file_size
        && identity.sha256 == expected_build.sha256
        && identity.machine == expected_build.machine
        && identity.optional_magic == expected_build.optional_magic
        && identity.image_size == expected_build.image_size;
}

// Reads at most 1 MiB from committed, readable memory in this process. Validation
// can race with game allocation, so ReadProcessMemory is still the final check.
// On false, the destination may contain a partial copy; safe_read<T> avoids it.
[[nodiscard]] inline bool safe_read_bytes(
    std::uintptr_t address, void* destination, std::size_t bytes) noexcept {
    if (bytes == 0) return true;
    if (!destination || address < 0x10000 || bytes > 1024 * 1024
        || address > std::numeric_limits<std::uintptr_t>::max() - bytes) return false;
    const auto end = address + bytes;
    for (auto cursor = address; cursor < end;) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region)
            || region.State != MEM_COMMIT
            || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
        switch (region.Protect & 0xff) {
        case PAGE_READONLY: case PAGE_READWRITE: case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READ: case PAGE_EXECUTE_READWRITE: case PAGE_EXECUTE_WRITECOPY:
            break;
        default: return false;
        }
        const auto base = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if (region.RegionSize > std::numeric_limits<std::uintptr_t>::max() - base) return false;
        const auto next = base + region.RegionSize;
        if (next <= cursor) return false;
        cursor = next < end ? next : end;
    }
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
                            destination, bytes, &copied) != FALSE && copied == bytes;
}

template<class T>
[[nodiscard]] bool safe_read(std::uintptr_t address, T& result) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    T temporary{};
    if (!safe_read_bytes(address, &temporary, sizeof(temporary))) return false;
    result = temporary;
    return true;
}

struct HostContext {
    std::uintptr_t module_base = 0;
    std::size_t module_size = 0;
    std::uint32_t main_thread_id = 0;
    std::filesystem::path plugin_directory;
    // UTF-8 message. The host only calls the SDK logger inside an SDK callback;
    // native UI hooks may also use this function, which then writes the file only.
    void (*log)(LogLevel, const char*) noexcept = nullptr;

    [[nodiscard]] std::uintptr_t address(std::size_t rva, std::size_t bytes = 1) const noexcept {
        if (rva >= module_size || bytes > module_size - rva
            || module_base > std::numeric_limits<std::uintptr_t>::max() - rva) return 0;
        return module_base + rva;
    }
};

struct FrameState {
    bool paused = true;
    std::uint64_t frame_number = 0;
};

// Implemented by game_adapter.cpp. All calls run on the SDK's documented main
// thread. initialize is called only after the exact executable and mapped PE
// identity are validated. shutdown must be idempotent, including after a failed
// or throwing initialize. No adapter callback may let a native fault escape.
bool adapter_initialize(const HostContext& host);
void adapter_tick(const FrameState& frame);
void adapter_shutdown() noexcept;

} // namespace ets2::native
