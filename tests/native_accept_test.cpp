#include "native_accept.hpp"
#include <array>
#include <cstring>
#include <cstdlib>
#include <iostream>
using namespace ets2::native;
namespace {
int checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
struct alignas(8) Offer {
    std::array<std::byte, 0xe0> data{};
    std::uintptr_t address() { return reinterpret_cast<std::uintptr_t>(data.data()); }
    template<class T> void put(std::size_t offset, T value) { std::memcpy(data.data()+offset, &value, sizeof(value)); }
};
}
int main() {
    Offer a, b;
    const char first[]="company.volatile.city_a", second[]="company.volatile.city_b";
    for (auto* offer : {&a, &b}) {
        offer->put(0x18, reinterpret_cast<std::uintptr_t>(first));
        offer->put<std::uint32_t>(0x20, sizeof(first)-1);
        offer->put<std::uint32_t>(0x60, 123);
        offer->put<std::uint32_t>(0xcc, 20);
        offer->put<float>(0xd0, 1.0f);
    }
    check(same_cargo_offer(a.address(), b.address()), "equal jobs match");
    b.put<std::uint64_t>(0, 12345);
    b.put<std::uint16_t>(0x56, 99);
    b.put<std::uint8_t>(0xc9, 99);
    check(same_cargo_offer(a.address(), b.address()), "unit headers and padding do not change job identity");
    b.put<std::uint32_t>(0xcc, 21);
    check(!same_cargo_offer(a.address(), b.address()), "different payload/pay rejected");
    b.put<std::uint32_t>(0xcc, 20);
    b.put(0x18, reinterpret_cast<std::uintptr_t>(second));
    check(!same_cargo_offer(a.address(), b.address()), "same-length different destination rejected");
    b.put(0x18, reinterpret_cast<std::uintptr_t>(first));
    b.put<std::uint32_t>(0x60, 124);
    check(!same_cargo_offer(a.address(), b.address()), "different seed rejected");
    b.put<std::uint32_t>(0x60, 123);
    b.put<float>(0xd0, 0.5f);
    check(!same_cargo_offer(a.address(), b.address()), "different fill ratio rejected");
    check(!same_cargo_offer(0, b.address()), "invalid memory rejected");
    b.put<float>(0xd0, 1.0f);
    b.put<std::uint32_t>(0x20, 513);
    check(!same_cargo_offer(b.address(), b.address()), "unbounded destination rejected");
    HostContext host;
    ets2::cargo_filter::native::OwnershipSnapshot snapshot;
    check(attach_for_offer(host, snapshot, 0, 0, 0) == AttachResult::unavailable,
          "uninitialized host never enters native code");
    host.module_base=1; host.main_thread_id=GetCurrentThreadId();
    snapshot.status=ets2::cargo_filter::native::OwnershipStatus::ready;
    check(attach_for_offer(host, snapshot, 42, 0, 0) == AttachResult::unavailable,
          "unowned trailer never enters native code");
    std::cout << checks << " acceptance checks passed\n";
}
