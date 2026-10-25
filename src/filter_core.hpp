#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace ets2::cargo_filter {

// These are opaque, exact IDs from one active-profile snapshot. The adapter
// must supply compatibility established by the game, including its capacity,
// loading and route restrictions. Body type alone is not compatibility.
using TrailerId = std::string;

struct OfferRow {
    std::string job_id;
    std::vector<TrailerId> compatible_trailer_ids;

    bool operator==(const OfferRow&) const = default;
};

struct Selection {
    // No ID means all owned trailers. A present ID means exactly that trailer.
    // An unavailable selected trailer never silently falls back to all trailers.
    std::optional<TrailerId> trailer_id;
};

enum class FilterStatus {
    matched,
    no_owned_trailers,
    selected_trailer_not_owned,
    no_matches,
};

struct FilterResult {
    FilterStatus status = FilterStatus::no_matches;
    // Indexes into the supplied engine-sorted rows, in their original order.
    // The adapter must consume these against the same immutable snapshot.
    std::vector<std::size_t> row_indices;
};

// Filters only: no sorting, job generation, de-duplication or game mutation.
// Distinct rows sharing a job ID can represent different quantities or pay;
// retain each matching row rather than conflating those variants.
[[nodiscard]] inline FilterResult filter_offers(
    std::span<const OfferRow> engine_sorted_rows,
    std::span<const TrailerId> owned_trailer_ids,
    const Selection& selection = {}) {
    if (owned_trailer_ids.empty()) {
        return {FilterStatus::no_owned_trailers, {}};
    }

    std::unordered_set<std::string_view> owned;
    owned.reserve(owned_trailer_ids.size());
    for (const auto& id : owned_trailer_ids) {
        owned.insert(id);
    }

    if (selection.trailer_id && !owned.contains(*selection.trailer_id)) {
        return {FilterStatus::selected_trailer_not_owned, {}};
    }

    FilterResult result;
    result.row_indices.reserve(engine_sorted_rows.size());
    for (std::size_t index = 0; index < engine_sorted_rows.size(); ++index) {
        for (const auto& compatible_id : engine_sorted_rows[index].compatible_trailer_ids) {
            const bool matches = selection.trailer_id
                ? compatible_id == *selection.trailer_id
                : owned.contains(compatible_id);
            if (matches) {
                result.row_indices.push_back(index);
                break;
            }
        }
    }
    result.status = result.row_indices.empty()
        ? FilterStatus::no_matches
        : FilterStatus::matched;
    return result;
}

} // namespace ets2::cargo_filter
