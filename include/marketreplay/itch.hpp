#pragma once

#include <cstdint>
#include <istream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace marketreplay {

class ReplayError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class MessageType : char {
    SystemEvent = 'S',
    AddOrder = 'A',
    AddOrderMpid = 'F',
    OrderExecuted = 'E',
    OrderExecutedWithPrice = 'C',
    OrderCancel = 'X',
    OrderDelete = 'D',
    OrderReplace = 'U',
};

struct Event {
    MessageType type{};
    std::uint16_t stock_locate{};
    std::uint64_t timestamp{};
    std::uint64_t order_ref{};
    std::uint64_t new_order_ref{};
    char side{};
    std::uint32_t shares{};
    std::uint32_t price{};
    std::string stock;
    char system_event_code{};
};

struct Order {
    std::uint64_t reference{};
    std::uint16_t stock_locate{};
    std::string stock;
    char side{};
    std::uint32_t shares{};
    std::uint32_t price{};
    std::uint64_t timestamp{};
};

struct Book {
    std::map<std::uint32_t, std::uint64_t, std::greater<>> bids;
    std::map<std::uint32_t, std::uint64_t> asks;
};

struct ReplayConfig {
    bool strict_time{false};
    std::uint64_t check_every{0};
};

class ReplayEngine {
public:
    explicit ReplayEngine(ReplayConfig config = {});

    void replay(std::istream& input);
    void apply(const Event& event);
    void validate() const;

    [[nodiscard]] std::string canonical_json() const;
    [[nodiscard]] std::string state_fingerprint() const;
    [[nodiscard]] std::uint64_t total_frames() const noexcept { return total_frames_; }
    [[nodiscard]] std::size_t active_orders() const noexcept { return orders_.size(); }

private:
    ReplayConfig config_;
    std::unordered_map<std::uint64_t, Order> orders_;
    std::unordered_map<std::string, Book> books_;
    std::map<char, std::uint64_t> message_counts_;
    std::uint64_t total_frames_{0};
    std::uint64_t last_timestamp_{0};
    std::uint64_t executed_shares_{0};
    std::uint64_t canceled_shares_{0};
    std::uint64_t replaced_orders_{0};

    void add_order(const Event& event);
    void reduce_order(std::uint64_t reference, std::uint32_t shares, bool execution);
    void delete_order(std::uint64_t reference);
    void replace_order(const Event& event);
    void change_level(const Order& order, std::int64_t delta);
    [[nodiscard]] std::string canonical_state_text() const;
};

[[nodiscard]] Event parse_message(const std::vector<std::uint8_t>& message);
[[nodiscard]] std::uint64_t fnv1a64(const std::string& value);
[[nodiscard]] std::string hex64(std::uint64_t value);

}  // namespace marketreplay
