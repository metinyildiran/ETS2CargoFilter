#include <windows.h>
#include <scssdk_telemetry.h>

#include <cstring>
#include <iostream>

namespace {
bool rejected_build = false;
unsigned registrations = 0;
SCSAPI_VOID log_message(const scs_log_type_t, const scs_string_t message) {
    if (message && std::strstr(message, "Executable mismatch")) rejected_build = true;
}
SCSAPI_RESULT register_event(const scs_event_t, const scs_telemetry_event_callback_t, const scs_context_t) {
    ++registrations;
    return SCS_RESULT_ok;
}
SCSAPI_RESULT unregister_event(const scs_event_t) { return SCS_RESULT_ok; }
}

int wmain(int count, wchar_t** arguments) {
    if (count != 2) return 1;
    const auto library = LoadLibraryW(arguments[1]);
    if (!library) { std::cerr << "DLL load failed: " << GetLastError() << '\n'; return 1; }
    const auto init = reinterpret_cast<scs_result_t (*)(scs_u32_t, const scs_telemetry_init_params_t*)>(
        reinterpret_cast<void*>(GetProcAddress(library, "scs_telemetry_init")));
    const auto shutdown = reinterpret_cast<void (*)()>(
        reinterpret_cast<void*>(GetProcAddress(library, "scs_telemetry_shutdown")));
    if (!init || !shutdown) return 1;
    scs_telemetry_init_params_v101_t parameters{};
    parameters.common.game_id = "eut2";
    parameters.common.game_name = "ETS2 test host";
    parameters.common.log = log_message;
    parameters.register_for_event = register_event;
    parameters.unregister_from_event = unregister_event;
    const auto unsupported = init(0, &parameters);
    const auto missing = init(SCS_TELEMETRY_VERSION_1_01, nullptr);
    const auto rejected = init(SCS_TELEMETRY_VERSION_1_01, &parameters);
    shutdown();
    shutdown(); // Failed initialization and repeat shutdown must be harmless.
    FreeLibrary(library);
    if (unsupported != SCS_RESULT_unsupported || missing != SCS_RESULT_invalid_parameter
        || rejected != SCS_RESULT_generic_error || !rejected_build || registrations != 0) return 1;
    std::cout << "DLL exports load; unsupported executable rejected before hook/event registration.\n";
    return 0;
}
