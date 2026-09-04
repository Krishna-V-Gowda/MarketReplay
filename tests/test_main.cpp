#include "marketreplay/itch.hpp"

#include <cstdint>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;

void put_u16(Bytes& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
    out.push_back(static_cast<std::uint8_t>(value));
}
void put_u32(Bytes& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}
void put_u48(Bytes& out, std::uint64_t value) {
    for (int shift = 40; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}
void put_u64(Bytes& out, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}
void header(Bytes& out, char type, std::uint16_t locate, std::uint64_t timestamp) {
    out.push_back(static_cast<std::uint8_t>(type));
    put_u16(out, locate);
    put_u16(out, 1);
    put_u48(out, timestamp);
}
Bytes add(std::uint64_t ref, char side, std::uint32_t shares, std::uint32_t price, std::uint64_t ts = 1, char type = 'A') {
    Bytes out; header(out, type, 7, ts); put_u64(out, ref); out.push_back(static_cast<std::uint8_t>(side));
    put_u32(out, shares); std::string stock = "ALPHA   "; out.insert(out.end(), stock.begin(), stock.end()); put_u32(out, price);
    if (type == 'F') { std::string mpid = "TEST"; out.insert(out.end(), mpid.begin(), mpid.end()); }
    return out;
}
Bytes reduce(char type, std::uint64_t ref, std::uint32_t shares, std::uint64_t ts = 2) {
    Bytes out; header(out, type, 7, ts); put_u64(out, ref); put_u32(out, shares);
    if (type == 'E') put_u64(out, 99);
    if (type == 'C') { put_u64(out, 99); out.push_back('Y'); put_u32(out, 10100); }
    return out;
}
Bytes del(std::uint64_t ref, std::uint64_t ts = 3) { Bytes out; header(out, 'D', 7, ts); put_u64(out, ref); return out; }
Bytes replace(std::uint64_t old_ref, std::uint64_t new_ref, std::uint32_t shares, std::uint32_t price, std::uint64_t ts = 3) {
    Bytes out; header(out, 'U', 7, ts); put_u64(out, old_ref); put_u64(out, new_ref); put_u32(out, shares); put_u32(out, price); return out;
}
std::string framed(const std::vector<Bytes>& messages) {
    std::string result;
    for (const auto& message : messages) {
        result.push_back(static_cast<char>(message.size() >> 8U)); result.push_back(static_cast<char>(message.size()));
        result.append(reinterpret_cast<const char*>(message.data()), static_cast<std::streamsize>(message.size()));
    }
    return result;
}
marketreplay::ReplayEngine replay(const std::vector<Bytes>& messages, marketreplay::ReplayConfig config = {}) {
    std::istringstream input(framed(messages)); marketreplay::ReplayEngine engine(config); engine.replay(input); return engine;
}
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
template <typename F> void require_error(F&& function) { bool raised = false; try { function(); } catch (const marketreplay::ReplayError&) { raised = true; } require(raised, "expected ReplayError"); }
}

int main() {
    struct Case { std::string name; std::function<void()> run; };
    std::vector<Case> cases;
    cases.push_back({"parse add", [] { auto e = marketreplay::parse_message(add(10, 'B', 100, 12345)); require(e.order_ref == 10 && e.stock == "ALPHA", "add fields"); }});
    cases.push_back({"parse attributed add", [] { auto e = marketreplay::parse_message(add(11, 'S', 50, 12400, 1, 'F')); require(static_cast<char>(e.type) == 'F', "F type"); }});
    cases.push_back({"execute partial", [] { auto e = replay({add(1, 'B', 100, 10000), reduce('E', 1, 40)}); auto j = e.canonical_json(); require(j.find("\"shares\":60") != std::string::npos, "remaining shares"); }});
    cases.push_back({"execute complete removes", [] { auto e = replay({add(1, 'B', 100, 10000), reduce('C', 1, 100)}); require(e.active_orders() == 0, "order removed"); }});
    cases.push_back({"cancel partial", [] { auto e = replay({add(1, 'S', 100, 10000), reduce('X', 1, 20)}); require(e.canonical_json().find("\"canceled_shares\":20") != std::string::npos, "cancel count"); }});
    cases.push_back({"delete", [] { auto e = replay({add(1, 'S', 100, 10000), del(1)}); require(e.active_orders() == 0, "deleted"); }});
    cases.push_back({"replace", [] { auto e = replay({add(1, 'B', 100, 10000), replace(1, 2, 80, 10050)}); auto j = e.canonical_json(); require(j.find("\"reference\":2") != std::string::npos && j.find("\"replaced_orders\":1") != std::string::npos, "replace state"); }});
    cases.push_back({"duplicate add rejected", [] { require_error([] { replay({add(1, 'B', 1, 1), add(1, 'B', 1, 1, 2)}); }); }});
    cases.push_back({"unknown reduction rejected", [] { require_error([] { replay({reduce('X', 99, 1)}); }); }});
    cases.push_back({"over reduction rejected", [] { require_error([] { replay({add(1, 'B', 10, 100), reduce('E', 1, 11)}); }); }});
    cases.push_back({"wrong message length rejected", [] { auto bytes = add(1, 'B', 10, 100); bytes.pop_back(); require_error([&] { (void)marketreplay::parse_message(bytes); }); }});
    cases.push_back({"unsupported type rejected", [] { Bytes bytes(12, 0); bytes[0] = 'Z'; require_error([&] { (void)marketreplay::parse_message(bytes); }); }});
    cases.push_back({"truncated prefix rejected", [] { std::istringstream input(std::string(1, '\0')); marketreplay::ReplayEngine e; require_error([&] { e.replay(input); }); }});
    cases.push_back({"truncated payload rejected", [] { std::string value{"\0\x24", 2}; value.append(3, 'x'); std::istringstream input(value); marketreplay::ReplayEngine e; require_error([&] { e.replay(input); }); }});
    cases.push_back({"strict timestamp rejected", [] { require_error([] { replay({add(1, 'B', 10, 100, 10), reduce('X', 1, 1, 9)}, {true, 0}); }); }});
    cases.push_back({"deterministic fingerprint", [] { auto a = replay({add(2, 'S', 20, 101), add(1, 'B', 10, 100, 2)}); auto b = replay({add(2, 'S', 20, 101), add(1, 'B', 10, 100, 2)}); require(a.state_fingerprint() == b.state_fingerprint() && a.canonical_json() == b.canonical_json(), "determinism"); }});
    cases.push_back({"periodic invariant checks", [] { auto e = replay({add(1, 'B', 10, 100), replace(1, 2, 7, 101), reduce('E', 2, 3)}, {false, 1}); e.validate(); require(e.active_orders() == 1, "active count"); }});

    std::size_t passed = 0;
    for (const auto& test_case : cases) {
        try { test_case.run(); ++passed; std::cout << "ok - " << test_case.name << '\n'; }
        catch (const std::exception& error) { std::cerr << "not ok - " << test_case.name << ": " << error.what() << '\n'; }
    }
    std::cout << passed << "/" << cases.size() << " tests passed\n";
    return passed == cases.size() ? 0 : 1;
}
