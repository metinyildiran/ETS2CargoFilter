#pragma once

#include "native_host.hpp"
#include "native_ownership.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ets2::native {

struct GeneratedOffer {
    std::uintptr_t company = 0;
    std::uintptr_t offer = 0;
    std::uintptr_t trailer = 0;
};

// Main-thread, screen-scoped owner of transient engine offers. Keep alive until
// every native row referring to an offer has been destroyed. Never persist it.
class NativeOfferBatch {
public:
    explicit NativeOfferBatch(const HostContext& host);
    ~NativeOfferBatch();
    NativeOfferBatch(const NativeOfferBatch&) = delete;
    NativeOfferBatch& operator=(const NativeOfferBatch&) = delete;

    bool generate(const cargo_filter::native::OwnershipSnapshot& snapshot,
                  std::uintptr_t trailer, std::uintptr_t source_constraint = 0);
    const std::vector<GeneratedOffer>& offers() const noexcept;
    const std::string& error() const noexcept;

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace ets2::native
