#include "marketreplay/itch.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <sstream>
#include <tuple>

namespace marketreplay {
namespace {

std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) {
        throw ReplayError("field exceeds message boundary");
    }
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) |
                                      static_cast<std::uint16_t>(bytes[offset + 1]));
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        throw ReplayError("field exceeds message boundary");
    }
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value = (value << 8U) | static_cast<std::uint32_t>(bytes[offset + i]);
    }
    return value;
}

std::uint64_t read_u48(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset + 6 > bytes.size()) {
        throw ReplayError("field exceeds message boundary");
    }
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 6; ++i) {
        value = (value << 8U) | static_cast<std::uint64_t>(bytes[offset + i]);
    }
    return value;
}

std::uint64_t read_u64(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset + 8 > bytes.size()) {
        throw ReplayError("field exceeds message boundary");
    }
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value = (value << 8U) | static_cast<std::uint64_t>(bytes[offset + i]);
    }
    return value;
}

std::string read_stock(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset + 8 > bytes.size()) {
        throw ReplayError("stock field exceeds message boundary");
    }
    std::string stock(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                      bytes.begin() + static_cast<std::ptrdiff_t>(offset + 8));
    while (!stock.empty() && stock.back() == ' ') {
        stock.pop_back();
    }
    if (stock.empty()) {
        throw ReplayError("stock symbol is empty");
    }
    return stock;
}

std::size_t expected_length(char type) {
    switch (type) {
        case 'S': return 12;
        case 'A': return 36;
        case 'F': return 40;
        case 'E': return 31;
        case 'C': return 36;
        case 'X': return 23;
        case 'D': return 19;
        case 'U': return 35;
        default: {
            std::ostringstream text;
            text << "unsupported ITCH message type byte: 0x" << std::hex
                 << std::setw(2) << std::setfill('0')
                 << static_cast<unsigned int>(static_cast<unsigned char>(type));
            throw ReplayError(text.str());
        }
    }
}

std::string json_escape(const std::string& input) {
    std::ostringstream out;
    for (const unsigned char ch : input) {
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20U) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(ch) << std::dec;
                } else {
                    out << static_cast<char>(ch);
                }
        }
    }
    return out.str();
}

}  // namespace

Event parse_message(const std::vector<std::uint8_t>& message) {
    if (message.empty()) {
        throw ReplayError("zero-length ITCH message");
    }
    const char type = static_cast<char>(message[0]);
    const auto expected = expected_length(type);
    if (message.size() != expected) {
        std::ostringstream text;
        text << "message " << type << " has length " << message.size()
             << "; expected " << expected;
        throw ReplayError(text.str());
    }

    Event event;
    event.type = static_cast<MessageType>(type);
    event.stock_locate = read_u16(message, 1);
    event.timestamp = read_u48(message, 5);

    switch (type) {
        case 'S':
            event.system_event_code = static_cast<char>(message[11]);
            break;
        case 'A':
        case 'F':
            event.order_ref = read_u64(message, 11);
            event.side = static_cast<char>(message[19]);
            event.shares = read_u32(message, 20);
            event.stock = read_stock(message, 24);
            event.price = read_u32(message, 32);
            break;
        case 'E':
        case 'C':
            event.order_ref = read_u64(message, 11);
            event.shares = read_u32(message, 19);
            break;
        case 'X':
            event.order_ref = read_u64(message, 11);
            event.shares = read_u32(message, 19);
            break;
        case 'D':
            event.order_ref = read_u64(message, 11);
            break;
        case 'U':
            event.order_ref = read_u64(message, 11);
            event.new_order_ref = read_u64(message, 19);
            event.shares = read_u32(message, 27);
            event.price = read_u32(message, 31);
            break;
        default:
            throw ReplayError("unreachable parser branch");
    }
    return event;
}

ReplayEngine::ReplayEngine(ReplayConfig config) : config_(config) {}

void ReplayEngine::replay(std::istream& input) {
    std::array<unsigned char, 2> prefix{};
    while (true) {
        input.read(reinterpret_cast<char*>(prefix.data()), 2);
        const auto prefix_read = input.gcount();
        if (prefix_read == 0 && input.eof()) {
            break;
        }
        if (prefix_read != 2) {
            throw ReplayError("truncated two-byte frame length");
        }
        const std::uint16_t length = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(prefix[0]) << 8U) |
            static_cast<std::uint16_t>(prefix[1]));
        if (length == 0) {
            throw ReplayError("zero-length frame");
        }
        std::vector<std::uint8_t> payload(length);
        input.read(reinterpret_cast<char*>(payload.data()), length);
        if (input.gcount() != static_cast<std::streamsize>(length)) {
            throw ReplayError("truncated ITCH frame payload");
        }
        apply(parse_message(payload));
    }
    validate();
}

void ReplayEngine::apply(const Event& event) {
    if (config_.strict_time && total_frames_ > 0 && event.timestamp < last_timestamp_) {
        throw ReplayError("timestamp regression under strict-time policy");
    }
    last_timestamp_ = std::max(last_timestamp_, event.timestamp);
    const char type = static_cast<char>(event.type);
    ++message_counts_[type];
    ++total_frames_;

    switch (event.type) {
        case MessageType::SystemEvent:
            break;
        case MessageType::AddOrder:
        case MessageType::AddOrderMpid:
            add_order(event);
            break;
        case MessageType::OrderExecuted:
        case MessageType::OrderExecutedWithPrice:
            reduce_order(event.order_ref, event.shares, true);
            break;
        case MessageType::OrderCancel:
            reduce_order(event.order_ref, event.shares, false);
            break;
        case MessageType::OrderDelete:
            delete_order(event.order_ref);
            break;
        case MessageType::OrderReplace:
            replace_order(event);
            break;
    }

    if (config_.check_every != 0 && total_frames_ % config_.check_every == 0) {
        validate();
    }
}

void ReplayEngine::change_level(const Order& order, std::int64_t delta) {
    auto& book = books_[order.stock];
    auto apply_delta = [delta](auto& levels, std::uint32_t price) {
        const auto current_it = levels.find(price);
        const std::uint64_t current = current_it == levels.end() ? 0 : current_it->second;
        if (delta < 0 && static_cast<std::uint64_t>(-delta) > current) {
            throw ReplayError("aggregate level quantity underflow");
        }
        const auto updated = delta >= 0
            ? current + static_cast<std::uint64_t>(delta)
            : current - static_cast<std::uint64_t>(-delta);
        if (updated == 0) {
            if (current_it != levels.end()) {
                levels.erase(current_it);
            }
        } else {
            levels[price] = updated;
        }
    };

    if (order.side == 'B') {
        apply_delta(book.bids, order.price);
    } else if (order.side == 'S') {
        apply_delta(book.asks, order.price);
    } else {
        throw ReplayError("order side must be B or S");
    }
    if (book.bids.empty() && book.asks.empty()) {
        books_.erase(order.stock);
    }
}

void ReplayEngine::add_order(const Event& event) {
    if (event.order_ref == 0 || event.shares == 0 || event.price == 0) {
        throw ReplayError("add order requires nonzero reference, shares, and price");
    }
    if (event.side != 'B' && event.side != 'S') {
        throw ReplayError("add order side must be B or S");
    }
    if (orders_.contains(event.order_ref)) {
        throw ReplayError("duplicate active order reference");
    }
    Order order{event.order_ref, event.stock_locate, event.stock, event.side,
                event.shares, event.price, event.timestamp};
    change_level(order, static_cast<std::int64_t>(order.shares));
    orders_.emplace(order.reference, std::move(order));
}

void ReplayEngine::reduce_order(std::uint64_t reference, std::uint32_t shares, bool execution) {
    if (shares == 0) {
        throw ReplayError("order reduction must be positive");
    }
    const auto it = orders_.find(reference);
    if (it == orders_.end()) {
        throw ReplayError("order reduction references unknown active order");
    }
    if (shares > it->second.shares) {
        throw ReplayError("order reduction exceeds remaining shares");
    }
    Order snapshot = it->second;
    change_level(snapshot, -static_cast<std::int64_t>(shares));
    if (execution) {
        executed_shares_ += shares;
    } else {
        canceled_shares_ += shares;
    }
    it->second.shares -= shares;
    if (it->second.shares == 0) {
        orders_.erase(it);
    }
}

void ReplayEngine::delete_order(std::uint64_t reference) {
    const auto it = orders_.find(reference);
    if (it == orders_.end()) {
        throw ReplayError("delete references unknown active order");
    }
    const Order snapshot = it->second;
    change_level(snapshot, -static_cast<std::int64_t>(snapshot.shares));
    orders_.erase(it);
}

void ReplayEngine::replace_order(const Event& event) {
    if (event.new_order_ref == 0 || event.shares == 0 || event.price == 0) {
        throw ReplayError("replace requires nonzero new reference, shares, and price");
    }
    const auto old_it = orders_.find(event.order_ref);
    if (old_it == orders_.end()) {
        throw ReplayError("replace references unknown active order");
    }
    if (orders_.contains(event.new_order_ref)) {
        throw ReplayError("replace new reference is already active");
    }
    Order replacement = old_it->second;
    change_level(replacement, -static_cast<std::int64_t>(replacement.shares));
    orders_.erase(old_it);
    replacement.reference = event.new_order_ref;
    replacement.shares = event.shares;
    replacement.price = event.price;
    replacement.timestamp = event.timestamp;
    change_level(replacement, static_cast<std::int64_t>(replacement.shares));
    orders_.emplace(replacement.reference, std::move(replacement));
    ++replaced_orders_;
}

void ReplayEngine::validate() const {
    std::unordered_map<std::string, Book> rebuilt;
    for (const auto& [reference, order] : orders_) {
        if (reference != order.reference || order.shares == 0 || order.price == 0 ||
            (order.side != 'B' && order.side != 'S')) {
            throw ReplayError("active-order invariant failed");
        }
        auto& book = rebuilt[order.stock];
        if (order.side == 'B') {
            book.bids[order.price] += order.shares;
        } else {
            book.asks[order.price] += order.shares;
        }
    }

    auto normalized = [](const auto& books) {
        std::map<std::string, std::pair<std::map<std::uint32_t, std::uint64_t>,
                                        std::map<std::uint32_t, std::uint64_t>>> result;
        for (const auto& [symbol, book] : books) {
            auto& target = result[symbol];
            for (const auto& [price, quantity] : book.bids) target.first[price] = quantity;
            for (const auto& [price, quantity] : book.asks) target.second[price] = quantity;
        }
        return result;
    };
    if (normalized(rebuilt) != normalized(books_)) {
        throw ReplayError("book-level aggregate invariant failed");
    }
}

std::string ReplayEngine::canonical_state_text() const {
    std::vector<Order> orders;
    orders.reserve(orders_.size());
    for (const auto& [_, order] : orders_) orders.push_back(order);
    std::sort(orders.begin(), orders.end(), [](const Order& lhs, const Order& rhs) {
        return lhs.reference < rhs.reference;
    });

    std::ostringstream out;
    out << "frames=" << total_frames_ << ";last=" << last_timestamp_
        << ";executed=" << executed_shares_ << ";canceled=" << canceled_shares_
        << ";replaced=" << replaced_orders_ << ';';
    for (const auto& [type, count] : message_counts_) out << "m" << type << '=' << count << ';';
    for (const auto& order : orders) {
        out << "o=" << order.reference << ',' << order.stock_locate << ',' << order.stock << ','
            << order.side << ',' << order.shares << ',' << order.price << ',' << order.timestamp << ';';
    }
    return out.str();
}

std::string ReplayEngine::state_fingerprint() const {
    return hex64(fnv1a64(canonical_state_text()));
}

std::string ReplayEngine::canonical_json() const {
    std::vector<Order> orders;
    orders.reserve(orders_.size());
    for (const auto& [_, order] : orders_) orders.push_back(order);
    std::sort(orders.begin(), orders.end(), [](const Order& lhs, const Order& rhs) {
        return lhs.reference < rhs.reference;
    });

    std::map<std::string, Book> books;
    for (const auto& [symbol, book] : books_) books.emplace(symbol, book);

    std::ostringstream out;
    out << '{';
    out << "\"active_orders\":" << orders.size() << ',';
    out << "\"books\":{";
    bool first_symbol = true;
    for (const auto& [symbol, book] : books) {
        if (!first_symbol) out << ',';
        first_symbol = false;
        out << '\"' << json_escape(symbol) << "\":{\"asks\":[";
        bool first = true;
        for (const auto& [price, quantity] : book.asks) {
            if (!first) out << ',';
            first = false;
            out << '[' << price << ',' << quantity << ']';
        }
        out << "],\"bids\":[";
        first = true;
        for (const auto& [price, quantity] : book.bids) {
            if (!first) out << ',';
            first = false;
            out << '[' << price << ',' << quantity << ']';
        }
        out << "]}";
    }
    out << "},";
    out << "\"canceled_shares\":" << canceled_shares_ << ',';
    out << "\"executed_shares\":" << executed_shares_ << ',';
    out << "\"fingerprint\":\"" << state_fingerprint() << "\",";
    out << "\"last_timestamp\":" << last_timestamp_ << ',';
    out << "\"message_counts\":{";
    bool first_count = true;
    for (const auto& [type, count] : message_counts_) {
        if (!first_count) out << ',';
        first_count = false;
        out << '\"' << type << "\":" << count;
    }
    out << "},";
    out << "\"orders\":[";
    bool first_order = true;
    for (const auto& order : orders) {
        if (!first_order) out << ',';
        first_order = false;
        out << "{\"price\":" << order.price
            << ",\"reference\":" << order.reference
            << ",\"shares\":" << order.shares
            << ",\"side\":\"" << order.side
            << "\",\"stock\":\"" << json_escape(order.stock)
            << "\",\"stock_locate\":" << order.stock_locate
            << ",\"timestamp\":" << order.timestamp << '}';
    }
    out << "],";
    out << "\"replaced_orders\":" << replaced_orders_ << ',';
    out << "\"total_frames\":" << total_frames_;
    out << '}';
    return out.str();
}

std::uint64_t fnv1a64(const std::string& value) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : value) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string hex64(std::uint64_t value) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}

}  // namespace marketreplay
