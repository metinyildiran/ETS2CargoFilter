#include "native_host.hpp"
#include <bcrypt.h>
#include <scssdk_telemetry.h>
#include <eurotrucks2/scssdk_eut2.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace ets2::native;
static_assert(sizeof(void*) == 8, "ETS2CargoFilter requires x64");

scs_log_t sdk_log = nullptr;
scs_telemetry_unregister_from_event_t unregister_event = nullptr;
HANDLE log_file = INVALID_HANDLE_VALUE;
SRWLOCK log_lock = SRWLOCK_INIT;
HostContext host;
FrameState frame;
bool adapter_active = false;
bool adapter_attempted = false;
thread_local bool sdk_call_window = false;
const int module_anchor = 0;
constexpr std::array<scs_event_t, 3> event_ids{
    SCS_TELEMETRY_EVENT_frame_end, SCS_TELEMETRY_EVENT_paused, SCS_TELEMETRY_EVENT_started
};
std::size_t registered_events = 0;

class SdkCallWindow {
    bool previous_ = sdk_call_window;
public:
    SdkCallWindow() noexcept { sdk_call_window = true; }
    ~SdkCallWindow() { sdk_call_window = previous_; }
};

class FileHandle {
public:
    HANDLE value = INVALID_HANDLE_VALUE;
    ~FileHandle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

void close_log() noexcept {
    AcquireSRWLockExclusive(&log_lock);
    if (log_file != INVALID_HANDLE_VALUE) {
        CloseHandle(log_file);
        log_file = INVALID_HANDLE_VALUE;
    }
    ReleaseSRWLockExclusive(&log_lock);
}

void log_message(LogLevel level, const char* text) noexcept {
    if (!text) return;
    const char* label = level == LogLevel::error ? "error" : level == LogLevel::warning ? "warning" : "info";
    char prefix[80]{};
    const int prefix_count = std::snprintf(prefix, sizeof(prefix), "[ETS2CargoFilter][%s] ", label);
    const auto length = std::strlen(text);
    AcquireSRWLockExclusive(&log_lock);
    if (log_file != INVALID_HANDLE_VALUE && prefix_count > 0) {
        DWORD written = 0;
        WriteFile(log_file, prefix, static_cast<DWORD>(prefix_count), &written, nullptr);
        // Stream large UTF-8 messages without cutting or rewriting their bytes.
        for (std::size_t offset = 0; offset < length;) {
            const auto count = static_cast<DWORD>(std::min<std::size_t>(length - offset, 65536));
            if (!WriteFile(log_file, text + offset, count, &written, nullptr) || written == 0) break;
            offset += written;
        }
        WriteFile(log_file, "\r\n", 2, &written, nullptr);
    }
    ReleaseSRWLockExclusive(&log_lock);
    if (sdk_log && sdk_call_window && GetCurrentThreadId() == host.main_thread_id) {
        const auto kind = level == LogLevel::error ? SCS_LOG_TYPE_error
            : level == LogLevel::warning ? SCS_LOG_TYPE_warning : SCS_LOG_TYPE_message;
        sdk_log(kind, text);
    }
}

std::filesystem::path module_path(HMODULE module) {
    std::vector<wchar_t> path(32768);
    const auto count = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!count || count >= path.size()) throw std::runtime_error("Cannot resolve module path");
    return std::filesystem::path(std::wstring(path.data(), count));
}

bool read_identity(const std::filesystem::path& path, BuildIdentity& identity) {
    FileHandle file;
    // Denying writes/deletion while hashing prevents an in-flight replacement.
    file.value = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file.value == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.value, &size) || size.QuadPart != static_cast<LONGLONG>(expected_build.file_size)) return false;
    identity.file_size = static_cast<std::uint64_t>(size.QuadPart);
    std::array<unsigned char, 4096> header{};
    DWORD count = 0;
    if (!ReadFile(file.value, header.data(), static_cast<DWORD>(header.size()), &count, nullptr)
        || count != header.size()) return false;
    IMAGE_DOS_HEADER dos{};
    std::memcpy(&dos, header.data(), sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0
        || static_cast<std::size_t>(dos.e_lfanew) > header.size() - sizeof(IMAGE_NT_HEADERS64)) return false;
    IMAGE_NT_HEADERS64 nt{};
    std::memcpy(&nt, header.data() + dos.e_lfanew, sizeof(nt));
    if (nt.Signature != IMAGE_NT_SIGNATURE) return false;
    identity.machine = nt.FileHeader.Machine;
    identity.optional_magic = nt.OptionalHeader.Magic;
    identity.image_size = nt.OptionalHeader.SizeOfImage;
    if (!SetFilePointerEx(file.value, {}, nullptr, FILE_BEGIN)) return false;

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    // This storage must outlive BCryptDestroyHash, including on early returns.
    std::vector<UCHAR> object;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    struct HashCleanup {
        BCRYPT_ALG_HANDLE& algorithm; BCRYPT_HASH_HANDLE& hash;
        ~HashCleanup() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    } cleanup{algorithm, hash};
    ULONG object_length = 0, result_length = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_length),
        sizeof(object_length), &result_length, 0) < 0 || !object_length || object_length > 1024 * 1024) return false;
    object.resize(object_length);
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_length, nullptr, 0, 0) < 0) return false;
    std::array<UCHAR, 65536> buffer{};
    std::uint64_t bytes_read = 0;
    for (;;) {
        if (!ReadFile(file.value, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) return false;
        if (!count) break;
        bytes_read += count;
        if (BCryptHashData(hash, buffer.data(), count, 0) < 0) return false;
    }
    return bytes_read == identity.file_size
        && BCryptFinishHash(hash, identity.sha256.data(), static_cast<ULONG>(identity.sha256.size()), 0) >= 0;
}

bool verify_mapped_image(std::uintptr_t base) noexcept {
    IMAGE_DOS_HEADER dos{};
    if (!safe_read(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew < 0 || dos.e_lfanew > 4096 - static_cast<LONG>(sizeof(IMAGE_NT_HEADERS64))) return false;
    IMAGE_NT_HEADERS64 nt{};
    return safe_read(base + static_cast<std::uintptr_t>(dos.e_lfanew), nt)
        && nt.Signature == IMAGE_NT_SIGNATURE
        && nt.FileHeader.Machine == expected_build.machine
        && nt.OptionalHeader.Magic == expected_build.optional_magic
        && nt.OptionalHeader.SizeOfImage == expected_build.image_size;
}

void stop_adapter() noexcept {
    adapter_active = false;
    if (adapter_attempted) {
        adapter_shutdown();
        adapter_attempted = false;
    }
}

void clear_registrations() noexcept {
    if (unregister_event) {
        while (registered_events > 0) unregister_event(event_ids[--registered_events]);
    }
}

SCSAPI_VOID telemetry_event(const scs_event_t event, const void*, const scs_context_t) {
    SdkCallWindow window;
    if (GetCurrentThreadId() != host.main_thread_id) {
        log_message(LogLevel::error, "Unexpected SDK callback thread; adapter callback skipped.");
        return;
    }
    if (event == SCS_TELEMETRY_EVENT_paused) frame.paused = true;
    else if (event == SCS_TELEMETRY_EVENT_started) frame.paused = false;
    else if (event == SCS_TELEMETRY_EVENT_frame_end && adapter_active) {
        ++frame.frame_number;
        try {
            adapter_tick(frame);
        } catch (const std::exception& error) {
            log_message(LogLevel::error, error.what());
            stop_adapter();
        } catch (...) {
            log_message(LogLevel::error, "Adapter exception; adapter disabled for this process.");
            stop_adapter();
        }
    }
}
} // namespace

SCSAPI_RESULT scs_telemetry_init(
    const scs_u32_t version, const scs_telemetry_init_params_t* const params) {
    SdkCallWindow window;
    if (version != SCS_TELEMETRY_VERSION_1_01) return SCS_RESULT_unsupported;
    if (!params) return SCS_RESULT_invalid_parameter;
    if (adapter_attempted || registered_events) return SCS_RESULT_already_registered;
    const auto* api = static_cast<const scs_telemetry_init_params_v101_t*>(params);
    if (!api->common.log || !api->common.game_id || !api->register_for_event || !api->unregister_from_event)
        return SCS_RESULT_invalid_parameter;
    sdk_log = api->common.log;
    host.main_thread_id = GetCurrentThreadId();
    host.log = log_message;
    unregister_event = api->unregister_from_event;
    try {
        HMODULE self = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&module_anchor), &self)) throw std::runtime_error("Cannot locate plugin module");
        host.plugin_directory = module_path(self).parent_path();
        log_file = CreateFileW((host.plugin_directory / L"ETS2CargoFilter.log").c_str(), FILE_APPEND_DATA,
                               FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (log_file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open ETS2CargoFilter.log beside the plugin");
        log_message(LogLevel::info, "Plugin host initialization; validating exact ETS2 executable.");
        if (std::strcmp(api->common.game_id, SCS_GAME_ID_EUT2) != 0)
            throw std::runtime_error("Unsupported game; native adapter was not initialized.");
        BuildIdentity identity{};
        host.module_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!read_identity(module_path(nullptr), identity) || !matches_expected_build(identity)
            || !verify_mapped_image(host.module_base))
            throw std::runtime_error("Executable mismatch: only verified ETS2 1.61.1.9 x64 is admitted; native adapter was not initialized.");
        host.module_size = identity.image_size;
        frame = {};
        adapter_attempted = true;
        if (!adapter_initialize(host)) throw std::runtime_error("Native adapter declined initialization.");
        for (const auto event : event_ids) {
            if (api->register_for_event(event, telemetry_event, nullptr) != SCS_RESULT_ok)
                throw std::runtime_error("Failed to register required SDK event callback.");
            ++registered_events;
        }
        adapter_active = true;
        log_message(LogLevel::info, "Exact build validated; adapter initialized and SDK callbacks registered.");
        return SCS_RESULT_ok;
    } catch (const std::exception& error) {
        log_message(LogLevel::error, error.what());
    } catch (...) {
        log_message(LogLevel::error, "Plugin initialization failed with an unexpected exception.");
    }
    clear_registrations();
    stop_adapter();
    close_log();
    sdk_log = nullptr;
    unregister_event = nullptr;
    return SCS_RESULT_generic_error;
}

SCSAPI_VOID scs_telemetry_shutdown() {
    SdkCallWindow window;
    clear_registrations();
    stop_adapter();
    log_message(LogLevel::info, "Plugin host shutdown complete.");
    close_log();
    sdk_log = nullptr;
    unregister_event = nullptr;
}
