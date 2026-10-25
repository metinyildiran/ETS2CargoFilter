#pragma once

// Resolved by the game's UI renderer against the active locale. Every custom
// token is supplied by locale/<locale>/local_module.ets2cargo_filter.sii.
namespace ets2::native::ui_text {
inline constexpr auto all_trailers = "@@ets2cf_all_trailers@@";
inline constexpr auto attached = "@@ets2cf_attached@@";
inline constexpr auto unknown_type = "@@ets2cf_unknown_type@@";
inline constexpr auto selector_failed = "@@ets2cf_selector_failed@@";
inline constexpr auto ownership_failed = "@@ets2cf_ownership_failed@@";
inline constexpr auto refresh_failed = "@@ets2cf_refresh_failed@@";
inline constexpr auto ownership_changed = "@@ets2cf_ownership_changed@@";
inline constexpr auto reopen_market = "@@ets2cf_reopen_market@@";
inline constexpr auto offers_failed = "@@ets2cf_offers_failed@@";
inline constexpr auto prepare_failed = "@@ets2cf_prepare_failed@@";
inline constexpr auto attach_failed = "@@ets2cf_attach_failed@@";
inline constexpr auto busy = "@@ets2cf_busy@@";
inline constexpr auto offer_changed = "@@ets2cf_offer_changed@@";
inline constexpr auto unavailable = "@@ets2cf_unavailable@@";
} // namespace ets2::native::ui_text
