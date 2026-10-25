#include "native_ui.hpp"

#include <cstring>
#include <limits>

namespace ets2::native {
namespace {

constexpr std::size_t find_control_rva = 0x385c60;
constexpr std::size_t clear_list_rva = 0x39e0f0;
constexpr std::size_t grow_items_rva = 0x3a1f90;
constexpr std::size_t assign_string_rva = 0xf2f60;
constexpr std::size_t update_geometry_rva = 0x3a0180;
constexpr std::size_t refresh_list_rva = 0x39fb00;
constexpr std::size_t select_dropdown_rva = 0x3c9090;
constexpr std::size_t sort_jobs_rva = 0x106d2a0;
constexpr std::size_t redraw_jobs_rva = 0x106c1d0;
constexpr std::size_t request_rebuild_rva = 0x1070f90;
constexpr std::size_t set_status_rva = 0x1070df0;
constexpr std::size_t max_dropdown_entries = 16384;

template<class Function>
Function entry(const HostContext& host, std::size_t rva) noexcept {
    return reinterpret_cast<Function>(host.address(rva));
}

template<class T>
void store(std::uintptr_t address, T value) noexcept {
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
}

std::uintptr_t find_unsafe(const HostContext& host, std::uintptr_t handler,
                          std::uint32_t id) {
    std::uintptr_t window = 0;
    const auto find = entry<std::uintptr_t (*)(std::uintptr_t, std::uint32_t)>(
        host, find_control_rva);
    if (!find || !safe_read(handler + 0x10, window) || !window) return 0;
    return find(window, id);
}

bool populate_unsafe(const HostContext& host, std::uintptr_t dropdown,
                     std::span<const std::string> labels, std::size_t selected) {
    const auto clear = entry<void (*)(std::uintptr_t)>(host, clear_list_rva);
    const auto grow = entry<bool (*)(std::uintptr_t, std::uint64_t)>(host, grow_items_rva);
    const auto assign = entry<void (*)(std::uintptr_t, const char* const*)>(host, assign_string_rva);
    const auto geometry = entry<void (*)(std::uintptr_t, bool)>(host, update_geometry_rva);
    const auto refresh = entry<void (*)(std::uintptr_t)>(host, refresh_list_rva);
    const auto select = entry<bool (*)(std::uintptr_t, std::uint64_t)>(host, select_dropdown_rva);
    if (!clear || !grow || !assign || !geometry || !refresh || !select) return false;
    std::uintptr_t list = 0;
    std::int32_t live_flags = 0;
    if (!safe_read(dropdown + 0xa0, list) || !list
        || !safe_read(list + 8, live_flags) || live_flags >= 0) return false;

    clear(list);
    for (std::size_t index = 0; index < labels.size(); ++index) {
        if (!grow(list + 0x160, 1)) {
            clear(list);
            return false;
        }
        std::uintptr_t data = 0;
        std::uint64_t count = 0;
        if (!safe_read(list + 0x168, data) || !data
            || !safe_read(list + 0x170, count) || count != index + 1) {
            clear(list);
            return false;
        }
        const auto item = data + index * 0x58;
        const char* label = labels[index].c_str();
        // grow() constructed both native strings. Assign the first in place;
        // the second remains the engine's empty string. The engine owns both.
        assign(item, &label);
        store<std::uint64_t>(item + 0x40, index);
        store<std::uint32_t>(item + 0x50, 0x00150001);
        std::uint8_t batching = 0;
        std::uint32_t displayed_count = 0;
        if (!safe_read(list + 0x1c8, batching)
            || !safe_read(list + 0x1bc, displayed_count)) return false;
        if (!batching) geometry(list, displayed_count != count);
        refresh(list);
    }
    return select(dropdown, selected);
}

} // namespace

bool NativeUi::on_main_thread() const noexcept {
    return host_.main_thread_id != 0 && GetCurrentThreadId() == host_.main_thread_id;
}

std::uintptr_t NativeUi::find_control(std::uintptr_t handler, std::uint32_t id) const noexcept {
    if (!on_main_thread() || !handler) return 0;
    __try {
        return find_unsafe(host_, handler, id);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool NativeUi::populate_trailer_dropdown(std::uintptr_t handler,
    std::span<const std::string> labels, std::size_t selected) const noexcept {
    if (!on_main_thread() || !handler || labels.empty()
        || labels.size() > max_dropdown_entries || selected >= labels.size()) return false;
    // Reject embedded NULs rather than silently displaying ambiguous identities.
    for (const auto& label : labels) {
        if (label.size() > 4096 || label.find('\0') != std::string::npos) return false;
    }
    __try {
        const auto dropdown = find_unsafe(host_, handler, trailer_dropdown_id);
        return dropdown && populate_unsafe(host_, dropdown, labels, selected);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::optional<std::size_t> NativeUi::selected_trailer_index(std::uintptr_t handler) const noexcept {
    const auto dropdown = find_control(handler, trailer_dropdown_id);
    std::uintptr_t list = 0;
    std::uint32_t selected = 0;
    std::uint64_t count = 0;
    if (!dropdown || !safe_read(dropdown + 0xa0, list) || !list
        || !safe_read(list + 0x1c0, selected) || !safe_read(list + 0x170, count)
        || count > max_dropdown_entries || selected >= count) return std::nullopt;
    return selected;
}

std::optional<std::size_t> NativeUi::decode_trailer_selection(
    std::uintptr_t control, std::uint32_t event_code,
    std::uintptr_t payload, std::size_t entry_count) noexcept {
    std::uint32_t control_id = 0;
    std::uint32_t selected = 0;
    if (event_code != dropdown_selection_event || !control || !payload
        || entry_count > max_dropdown_entries
        || !safe_read(control + 0x14, control_id) || control_id != trailer_dropdown_id
        || !safe_read(payload + 0x10, selected) || selected >= entry_count) return std::nullopt;
    return selected;
}

bool NativeUi::sort_and_redraw(std::uintptr_t handler) const noexcept {
    if (!on_main_thread() || !handler) return false;
    const auto sort = entry<void (*)(std::uintptr_t)>(host_, sort_jobs_rva);
    const auto redraw = entry<void (*)(std::uintptr_t, bool)>(host_, redraw_jobs_rva);
    if (!sort || !redraw) return false;
    __try {
        sort(handler);
        redraw(handler, true);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool NativeUi::request_rebuild(std::uintptr_t handler) const noexcept {
    if (!on_main_thread() || !handler) return false;
    const auto rebuild = entry<void (*)(std::uintptr_t)>(host_, request_rebuild_rva);
    if (!rebuild) return false;
    __try {
        rebuild(handler);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool NativeUi::set_status(std::uintptr_t handler, const char* message) const noexcept {
    if (!on_main_thread() || !handler || !message) return false;
    const auto status = entry<void (*)(std::uintptr_t, const char* const*)>(host_, set_status_rva);
    if (!status) return false;
    const char* text = message;
    __try {
        status(handler, &text);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace ets2::native
