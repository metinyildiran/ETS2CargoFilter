#pragma once

#include "native_host.hpp"

#include <optional>
#include <span>
#include <string>

namespace ets2::native {

inline constexpr std::uint32_t trailer_dropdown_id = 9470;
inline constexpr std::uint32_t dropdown_selection_event = 21;
inline constexpr std::size_t cargo_handler_vtable_rva = 0x23a03e8;

// These are version-specific in-process calls, established by static evidence
// for expected_build. They do not install hooks or validate the file identity.
// The host must validate that identity before constructing this binding and
// invoke it only while the native handler/window is alive on the main thread.
// Native calls have not yet been exercised in a running game.
class NativeUi {
public:
    explicit NativeUi(const HostContext& host) noexcept : host_(host) {}

    [[nodiscard]] std::uintptr_t find_control(
        std::uintptr_t handler, std::uint32_t id) const noexcept;

    // Entry zero may be the caller's "all owned trailers" label. Entry IDs are
    // their zero-based positions. Labels are copied into game-owned strings.
    // Selecting an entry can synchronously emit an event: the caller must guard
    // against reentering its filter handler while this function runs.
    [[nodiscard]] bool populate_trailer_dropdown(
        std::uintptr_t handler, std::span<const std::string> labels,
        std::size_t selected) const noexcept;

    [[nodiscard]] std::optional<std::size_t> selected_trailer_index(
        std::uintptr_t handler) const noexcept;

    // Use against the very same populated snapshot. This only recognizes the
    // new control; existing filter/sort events must reach the original handler.
    [[nodiscard]] static std::optional<std::size_t> decode_trailer_selection(
        std::uintptr_t control, std::uint32_t event_code,
        std::uintptr_t payload, std::size_t entry_count) noexcept;

    // Calls the existing sorter then its existing redraw path. Does not change
    // the player's sorting setting or generate/construct native offer rows.
    [[nodiscard]] bool sort_and_redraw(std::uintptr_t handler) const noexcept;

    // Requests the same asynchronous provider refresh used by the existing
    // cargo filter. Native update discards pending stale results first.
    [[nodiscard]] bool request_rebuild(std::uintptr_t handler) const noexcept;

    // Native empty-results message: hides job portal/pagination and displays
    // text in the existing message label (381). A completed rebuild restores
    // normal results. This is not a toast or an accept-button caption.
    [[nodiscard]] bool set_status(std::uintptr_t handler,
                                  const char* message) const noexcept;

private:
    [[nodiscard]] bool on_main_thread() const noexcept;
    const HostContext& host_;
};

} // namespace ets2::native
