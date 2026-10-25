#include "native_host.hpp"
#include "native_hooks.hpp"
#include "native_offers.hpp"
#include "native_rows.hpp"
#include "native_ui.hpp"
#include "native_accept.hpp"
#include "native_market_access.hpp"
#include "ui_text.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ets2::native {
namespace {
using namespace cargo_filter::native;
using Address = std::uintptr_t;
constexpr std::size_t handler_table = 0x23a03e8;
constexpr std::size_t provider_table = 0x23a03b8;
constexpr std::size_t desktop_table = 0x236f220;

struct BatchRecord {
    Address trailer = 0;
    Address source = 0;
    std::uint32_t minute = 0;
    std::shared_ptr<NativeOfferBatch> batch;
};
struct Screen {
    OwnershipSnapshot ownership;
    std::size_t selection = 0;
    bool populating = false;
    std::vector<BatchRecord> batches;
    const char* status = nullptr;
};
struct TaskContext {
    bool include_active = true;
    std::vector<std::shared_ptr<NativeOfferBatch>> batches;
};
struct Adapter {
    HostContext host;
    std::array<VtableSlotPatch, 11> patches;
    CargoMenuAccess menu_access;
    std::unordered_map<Address, Screen> screens; // game main thread only
    std::mutex tasks_mutex;
    std::unordered_map<Address, std::shared_ptr<TaskContext>> tasks;
    bool hooks_exposed = false;
    explicit Adapter(const HostContext& context) : host(context) {}
};
// Explicit lifetime: in the exceptional shutdown case native workers may still
// hold callbacks. Keep state and the pinned module valid until process exit.
Adapter* adapter = nullptr;

template<class F> F function(std::size_t rva) noexcept {
    return reinterpret_cast<F>(adapter->host.address(rva));
}
void log_error(const char* text) noexcept {
    if (adapter && adapter->host.log) adapter->host.log(LogLevel::error, text);
}
bool main_thread() noexcept {
    return adapter && GetCurrentThreadId() == adapter->host.main_thread_id;
}

bool supported_scheduler() noexcept {
    Address scheduler = 0, submit = 0, join = 0;
    return safe_read(adapter->host.address(0x36ad978), scheduler) && scheduler
        && safe_read(scheduler + 8, submit) && submit == adapter->host.address(0x167e00)
        && safe_read(scheduler + 0x28, join) && join == adapter->host.address(0x167e40);
}

void populate(Address handler, Screen& screen) {
    std::vector<std::string> labels{ui_text::all_trailers};
    for (const auto& trailer : screen.ownership.trailers) {
        labels.push_back(trailer.label + (trailer.address == screen.ownership.active_trailer
            ? std::string(" (") + ui_text::attached + ")" : ""));
    }
    screen.populating = true;
    const auto success = NativeUi(adapter->host).populate_trailer_dropdown(handler, labels, screen.selection);
    screen.populating = false;
    if (!success) screen.status = ui_text::selector_failed;
}

void on_init(Address handler) {
    function<void (*)(Address)>(0x11a2ea0)(handler);
    if (!main_thread() || !NativeUi(adapter->host).find_control(handler, trailer_dropdown_id)) return;
    try {
        auto& screen = adapter->screens[handler];
        screen.ownership = read_ownership(adapter->host.module_base);
        if (screen.ownership.status != OwnershipStatus::ready)
            screen.status = ui_text::ownership_failed;
        populate(handler, screen);
        if (!screen.status && !NativeUi(adapter->host).request_rebuild(handler))
            screen.status = ui_text::refresh_failed;
        char message[128]{};
        std::snprintf(message, sizeof(message), "Cargo screen initialized; owned trailers=%zu.", screen.ownership.trailers.size());
        if (adapter->host.log) adapter->host.log(LogLevel::info, message);
    } catch (...) { log_error("Cargo screen initialization failed."); }
}

bool on_event(Address handler, Address control, std::uint32_t event, Address payload) {
    if (main_thread()) {
        const auto found = adapter->screens.find(handler);
        if (found != adapter->screens.end()) {
            auto& screen = found->second;
            const auto selected = NativeUi::decode_trailer_selection(control, event, payload,
                                                                     screen.ownership.trailers.size() + 1);
            if (selected) {
                if (!screen.populating && screen.selection != *selected) {
                    screen.selection = *selected;
                    screen.status = nullptr;
                    // Native rebuild flag path retains the existing sort setting
                    // and waits for pending provider work before replacing rows.
                    if (!NativeUi(adapter->host).request_rebuild(handler))
                        screen.status = ui_text::refresh_failed;
                }
                return true;
            }
        }
    }
    return function<bool (*)(Address, Address, std::uint32_t, Address)>(0x11a2fe0)(handler, control, event, payload);
}

Address on_factory(Address handler) {
    const auto task = function<Address (*)(Address)>(0x11a2b30)(handler);
    if (!task || !main_thread()) return task;
    const auto found = adapter->screens.find(handler);
    if (found == adapter->screens.end()) return task;
    auto& screen = found->second;
    try {
        auto context = std::make_shared<TaskContext>();
        auto snapshot = read_ownership(adapter->host.module_base);
        Address selected_trailer = 0;
        if (screen.selection && screen.selection <= screen.ownership.trailers.size())
            selected_trailer = screen.ownership.trailers[screen.selection - 1].address;
        const bool same_profile = snapshot.status == OwnershipStatus::ready
            && snapshot.economy == screen.ownership.economy && snapshot.player == screen.ownership.player;
        const bool selection_exists = !selected_trailer || std::any_of(snapshot.trailers.begin(), snapshot.trailers.end(),
            [selected_trailer](const auto& trailer) { return trailer.address == selected_trailer; });
        context->include_active = !selected_trailer || selected_trailer == snapshot.active_trailer;
        Address source = 0;
        std::uint32_t minute = 0;
        if (!supported_scheduler() || !same_profile || !selection_exists || !safe_read(task + 0x350, source)
            || !safe_read(snapshot.economy + 0x19c, minute)) {
            context->include_active = false;
            screen.status = ui_text::ownership_changed;
        } else {
            for (const auto& trailer : snapshot.trailers) {
                if (trailer.address == snapshot.active_trailer || (selected_trailer && trailer.address != selected_trailer)) continue;
                const auto cached = std::find_if(screen.batches.begin(), screen.batches.end(), [&](const auto& record) {
                    return record.trailer == trailer.address && record.source == source && record.minute == minute;
                });
                if (cached != screen.batches.end()) {
                    context->batches.push_back(cached->batch);
                    continue;
                }
                // Retired batches remain alive while old native rows can refer to
                // them. Bound memory until the screen is closed and rows released.
                if (screen.batches.size() >= 128) {
                    screen.status = ui_text::reopen_market;
                    break;
                }
                auto batch = std::make_shared<NativeOfferBatch>(adapter->host);
                if (!batch->generate(snapshot, trailer.address, source)) {
                    log_error(batch->error().c_str());
                    screen.status = ui_text::offers_failed;
                    break;
                }
                screen.batches.push_back({trailer.address, source, minute, batch});
                char message[128]{};
                std::snprintf(message, sizeof(message), "Generated trailer offer batch; offers=%zu.", batch->offers().size());
                if (adapter->host.log) adapter->host.log(LogLevel::info, message);
                context->batches.push_back(std::move(batch));
            }
        }
        std::lock_guard lock(adapter->tasks_mutex);
        adapter->tasks.emplace(task, std::move(context));
    } catch (...) {
        screen.status = ui_text::prepare_failed;
        log_error("Cargo provider preparation failed.");
    }
    return task;
}

void on_execute(Address task) {
    std::shared_ptr<TaskContext> context;
    {
        std::lock_guard lock(adapter->tasks_mutex);
        const auto found = adapter->tasks.find(task);
        if (found != adapter->tasks.end()) context = found->second;
    }
    if (!context) {
        function<void (*)(Address)>(0x11a32b0)(task);
        return;
    }
    NativeRows rows(adapter->host);
    if (context->include_active) {
        function<void (*)(Address)>(0x11a32b0)(task);
        std::uint8_t attached = 0;
        if (!safe_read(task + 0x358, attached)) return;
        if (!attached && !rows.prepare_city_keys(task)) return;
    } else if (!rows.prepare_city_keys(task)) return;
    for (const auto& batch : context->batches) {
        for (const auto& row : batch->offers()) {
            if (rows.append(task, row.company, row.offer) == AppendRowResult::failed) {
                log_error("Native cargo row append failed.");
                return;
            }
        }
    }
    if (!context->batches.empty() && !rows.calculate_pay(task)) log_error("Native cargo pay calculation failed.");
    // The native scheduler sets task state=3 only after this callback returns.
    // Native handler then transfers rows and invokes its original sorter.
}

Address on_task_destroy(Address task, std::uint32_t flags) {
    std::shared_ptr<TaskContext> retained;
    {
        std::lock_guard lock(adapter->tasks_mutex);
        const auto found = adapter->tasks.find(task);
        if (found != adapter->tasks.end()) {
            retained = std::move(found->second);
            adapter->tasks.erase(found);
        }
    }
    const auto result = function<Address (*)(Address, std::uint32_t)>(0x11a3270)(task, flags);
    return result;
}

bool on_accept(Address handler) {
    if (main_thread()) {
        const auto found = adapter->screens.find(handler);
        if (found != adapter->screens.end()) {
            Address data = 0;
            std::uint64_t count = 0;
            std::int64_t selected = -1;
            if (!safe_read(handler + 0x2a8, data) || !safe_read(handler + 0x2b0, count)
                || !safe_read(handler + 0x308, selected)) return false;
            if (data && count <= 1000000 && selected >= 0 && static_cast<std::uint64_t>(selected) < count) {
                Address offer = 0;
                const auto row_address = data + static_cast<std::uint64_t>(selected) * 0xc8;
                if (!safe_read(row_address + 8, offer)) return false;
                for (const auto& record : found->second.batches) {
                    const auto match = std::find_if(record.batch->offers().begin(), record.batch->offers().end(),
                        [offer](const auto& row) { return row.offer == offer; });
                    if (match != record.batch->offers().end()) {
                        // Hold previews across native calls, including possible
                        // screen teardown. The ordinary guard handles job dialogs.
                        const auto retained = record.batch;
                        const auto trailer = match->trailer;
                        const auto company = match->company;
                        if (!function<bool (*)(Address)>(0x106eb60)(handler)) return false;
                        Address pending = 0;
                        std::uint32_t task_state = 0;
                        if (!safe_read(handler + 0x328, pending)
                            || (pending && (!safe_read(pending + 0x20, task_state) || task_state != 3)))
                            return false;
                        AttachResult result = AttachResult::unavailable;
                        try {
                            const auto current = read_ownership(adapter->host.module_base);
                            if (current.player == found->second.ownership.player)
                                result = attach_for_offer(adapter->host, current, trailer, company, row_address);
                        } catch (...) { log_error("Trailer acceptance preparation failed."); }
                        if (result == AttachResult::ready) {
                            if (adapter->host.log) adapter->host.log(LogLevel::info,
                                "Selected trailer attached; matching native company offer ready for acceptance.");
                            return function<bool (*)(Address)>(0x11a2c80)(handler);
                        }
                        const char* message = ui_text::attach_failed;
                        if (result == AttachResult::busy)
                            message = ui_text::busy;
                        else if (result == AttachResult::offer_changed)
                            message = ui_text::offer_changed;
                        else if (result == AttachResult::unavailable)
                            message = ui_text::unavailable;
                        log_error(message);
                        const auto screen_now = adapter->screens.find(handler);
                        if (screen_now != adapter->screens.end()) screen_now->second.status = message;
                        static_cast<void>(NativeUi(adapter->host).set_status(handler, message));
                        return false;
                    }
                }
            }
        }
    }
    return function<bool (*)(Address)>(0x11a2c80)(handler);
}

Address on_screen_destroy(Address handler, std::uint32_t flags) {
    // Native teardown destroys pending task and current rows first. Batch owner
    // arrays must not invalidate their offer units while those rows are live.
    const auto result = function<Address (*)(Address, std::uint32_t)>(0x11a28d0)(handler, flags);
    adapter->screens.erase(handler);
    return result;
}

void on_screen_close(Address handler) {
    function<void (*)(Address)>(0x11a2910)(handler);
    // Unlike the deleting destructor, this native teardown explicitly cancels
    // and joins its provider before destroying the rows.
    adapter->screens.erase(handler);
}

void on_update(Address handler) {
    function<void (*)(Address)>(0x11a2a10)(handler);
    if (!main_thread()) return;
    const auto found = adapter->screens.find(handler);
    if (found != adapter->screens.end() && found->second.status)
        static_cast<void>(NativeUi(adapter->host).set_status(handler, found->second.status));
}

void refresh_menu_access(bool force) noexcept {
    if (!main_thread()) return;
    try { adapter->menu_access.refresh(adapter->host, force); }
    catch (...) { log_error("Cargo Market availability refresh failed."); }
}

void on_desktop_init(Address handler) {
    function<void (*)(Address)>(0xfe4e80)(handler);
    refresh_menu_access(true);
}

void on_desktop_update(Address handler) {
    function<void (*)(Address)>(0xfe58b0)(handler);
    refresh_menu_access(false);
}

struct HookDefinition { std::size_t slot; std::size_t original; Address replacement; };
}

bool adapter_initialize(const HostContext& host) {
    if (adapter) return false;
    adapter = new Adapter(host);
    const std::array definitions{
        HookDefinition{handler_table + 0xc8, 0x11a2ea0, reinterpret_cast<Address>(&on_init)},
        HookDefinition{handler_table + 0x68, 0x11a2fe0, reinterpret_cast<Address>(&on_event)},
        HookDefinition{handler_table + 0x1e8, 0x11a2b30, reinterpret_cast<Address>(&on_factory)},
        HookDefinition{handler_table + 0x220, 0x11a2c80, reinterpret_cast<Address>(&on_accept)},
        HookDefinition{handler_table, 0x11a28d0, reinterpret_cast<Address>(&on_screen_destroy)},
        HookDefinition{provider_table + 8, 0x11a32b0, reinterpret_cast<Address>(&on_execute)},
        HookDefinition{provider_table, 0x11a3270, reinterpret_cast<Address>(&on_task_destroy)},
        HookDefinition{handler_table + 8, 0x11a2910, reinterpret_cast<Address>(&on_screen_close)},
        HookDefinition{handler_table + 0x1f0, 0x11a2a10, reinterpret_cast<Address>(&on_update)},
        HookDefinition{desktop_table + 0xc8, 0xfe4e80, reinterpret_cast<Address>(&on_desktop_init)},
        HookDefinition{desktop_table + 0x60, 0xfe58b0, reinterpret_cast<Address>(&on_desktop_update)}
    };
    // Validate every slot before modifying any one of them.
    for (const auto& hook : definitions) {
        Address current = 0;
        if (!safe_read(host.address(hook.slot), current) || current != host.address(hook.original)) {
            log_error("Native UI vtable differs from the supported build or another plugin owns a slot.");
            adapter_shutdown();
            return false;
        }
    }
    HMODULE pinned_module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&on_init), &pinned_module)) {
        adapter_shutdown();
        return false;
    }
    adapter->hooks_exposed = true;
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const auto& hook = definitions[index];
        const auto status = adapter->patches[index].install(host.address(hook.slot), host.address(hook.original), hook.replacement);
        if (status != HookStatus::installed) {
            log_error(hook_status_name(status));
            adapter_shutdown();
            return false;
        }
    }
    if (host.log) host.log(LogLevel::info, "Cargo filter revision 4 initialized for ETS2 1.61.1.9; native localization for all 43 game locales.");
    return true;
}

void adapter_tick(const FrameState&) {
    // Screen state is driven by native screen/provider callbacks so it also
    // works while simulation/telemetry is paused in the market menu.
}

void adapter_shutdown() noexcept {
    if (!adapter) return;
    bool restored = true;
    for (auto item = adapter->patches.rbegin(); item != adapter->patches.rend(); ++item) {
        const auto result = item->restore();
        if (result != HookStatus::restored && result != HookStatus::not_installed) restored = false;
    }
    bool pending_tasks = false;
    {
        std::lock_guard lock(adapter->tasks_mutex);
        pending_tasks = !adapter->tasks.empty();
    }
    if (!adapter->screens.empty() || pending_tasks || !restored) {
        // SDK shutdown can follow destruction of the unit system. Do not call
        // engine destructors through stale objects at process teardown.
        log_error("Native state remains at shutdown; retaining process-lifetime state.");
        return;
    }
    // Hooks may already be on another thread's stack even after every slot is
    // restored. State is tiny when screens/tasks are empty; keep it with the
    // pinned DLL instead of racing a callback returning through its mutex.
    if (adapter->hooks_exposed) return;
    delete adapter;
    adapter = nullptr;
}
} // namespace ets2::native
