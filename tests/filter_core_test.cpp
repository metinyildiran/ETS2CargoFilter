#include "../src/filter_core.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
using namespace ets2::cargo_filter;

int checks = 0;

void check(bool condition, std::string_view description) {
    ++checks;
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void expect(const FilterResult& actual, FilterStatus status,
            std::vector<std::size_t> indices, std::string_view description) {
    check(actual.status == status && actual.row_indices == indices, description);
}
} // namespace

int main() {
    // The engine's order deliberately differs from lexical job-ID order.
    // The two same-job rows can represent distinct quantities/pay variants.
    const std::vector<OfferRow> rows{
        {"z-job", {"owned-B", "foreign-C"}},
        {"foreign-job", {"foreign-C"}},
        {"same-job", {"owned-A"}},
        {"a-job", {"owned-A", "owned-B"}},
        {"same-job", {"owned-B"}},
        {"no-compatibility", {}},
        {"duplicate-compatibility", {"owned-A", "owned-A", "owned-B"}},
        {"case-sensitive", {"OWNED-A"}},
    };
    const std::vector<TrailerId> owned{"owned-A", "owned-B"};
    const auto original_rows = rows;
    const auto original_owned = owned;

    expect(filter_offers(rows, owned), FilterStatus::matched, {0, 2, 3, 4, 6},
           "all owned retains engine order and distinct same-job rows");
    expect(filter_offers(rows, owned, Selection{"owned-A"}),
           FilterStatus::matched, {2, 3, 6}, "one exact trailer preserves order");
    expect(filter_offers(rows, owned, Selection{"owned-B"}),
           FilterStatus::matched, {0, 3, 4, 6}, "another exact trailer changes only membership");
    expect(filter_offers(rows, owned, Selection{"foreign-C"}),
           FilterStatus::selected_trailer_not_owned, {},
           "compatible foreign trailer cannot be selected as owned");
    expect(filter_offers(rows, owned, Selection{"removed-trailer"}),
           FilterStatus::selected_trailer_not_owned, {},
           "removed selection does not fall back to all owned");
    expect(filter_offers(rows, owned, Selection{"OWNED-A"}),
           FilterStatus::selected_trailer_not_owned, {}, "IDs are exact and case sensitive");

    const std::vector<TrailerId> no_owned;
    expect(filter_offers(rows, no_owned), FilterStatus::no_owned_trailers, {},
           "empty ownership shows no offers");
    expect(filter_offers(rows, no_owned, Selection{"owned-A"}),
           FilterStatus::no_owned_trailers, {}, "empty ownership takes precedence over stale selection");

    const std::vector<TrailerId> unrelated_owned{"owned-D"};
    expect(filter_offers(rows, unrelated_owned), FilterStatus::no_matches, {},
           "ownership alone does not infer compatibility");
    expect(filter_offers(rows, unrelated_owned, Selection{"owned-D"}),
           FilterStatus::no_matches, {}, "valid selection can have no compatible jobs");
    const std::vector<OfferRow> no_rows;
    expect(filter_offers(no_rows, owned), FilterStatus::no_matches, {},
           "empty engine list is a valid no-match result");

    const std::vector<TrailerId> duplicated_owned{"owned-A", "owned-B", "owned-A"};
    expect(filter_offers(rows, duplicated_owned), FilterStatus::matched, {0, 2, 3, 4, 6},
           "duplicate IDs never duplicate result rows");
    const std::vector<TrailerId> next_profile{"foreign-C"};
    expect(filter_offers(rows, next_profile), FilterStatus::matched, {0, 1},
           "ownership is supplied per snapshot with no previous-profile leakage");
    expect(filter_offers(rows, next_profile, Selection{"owned-A"}),
           FilterStatus::selected_trailer_not_owned, {},
           "selection from a previous profile cannot leak into the next profile");

    check(rows == original_rows && owned == original_owned,
          "all calls leave input rows and ownership unchanged");
    std::cout << "Passed " << checks << " filter-core checks.\n";
    return EXIT_SUCCESS;
}
