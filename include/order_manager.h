#ifndef ORDER_MANAGER_H
#define ORDER_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include "deribit_api_client.h"

namespace deribit {

using json = nlohmann::json;

/**
 * @enum OrderType
 * @brief Types of orders that can be placed
 */
enum class OrderType {
    MARKET,
    LIMIT,
    STOP_MARKET,
    STOP_LIMIT
};

/**
 * @enum OrderDirection
 * @brief Direction of the order (buy or sell)
 */
enum class OrderDirection {
    BUY,
    SELL
};

/**
 * @enum TimeInForce
 * @brief Time in force options for orders
 */
enum class TimeInForce {
    GOOD_TIL_CANCELLED,
    FILL_OR_KILL,
    IMMEDIATE_OR_CANCEL
};

/**
 * @struct Order
 * @brief Represents an order in the system
 */
struct Order {
    std::string order_id;
    std::string instrument_name;
    OrderType type;
    OrderDirection direction;
    double price;
    double amount;
    TimeInForce time_in_force;
    std::string status;
    std::string created_at;
    std::string last_updated_at;
};

/**
 * @struct Position
 * @brief Represents a position in the system
 */
struct Position {
    std::string instrument_name;
    double size;
    double entry_price;
    double mark_price;
    double liquidation_price;
    double unrealized_pnl;
    double realized_pnl;
};

/**
 * @struct OrderBook
 * @brief Represents an orderbook for an instrument
 */
struct OrderBook {
    std::string instrument_name;
    std::vector<std::pair<double, double>> bids; // price, size
    std::vector<std::pair<double, double>> asks; // price, size
    std::string timestamp;
};

/**
 * @class OrderManager
 * @brief Manages orders and positions on Deribit
 */
class OrderManager {
public:
    /**
     * @brief Constructor
     * @param api_client Pointer to an initialized ApiClient
     */
    explicit OrderManager(std::shared_ptr<ApiClient> api_client);

    /**
     * @brief Place a new order
     * @param instrument_name The name of the instrument to trade
     * @param type The type of order
     * @param direction Buy or sell
     * @param amount The amount to trade
     * @param price The price for limit orders (ignored for market orders)
     * @param time_in_force Time in force option
     * @return The order ID if successful, empty string otherwise
     */
    std::string place_order(const std::string& instrument_name,
                           OrderType type,
                           OrderDirection direction,
                           double amount,
                           double price = 0.0,
                           TimeInForce time_in_force = TimeInForce::GOOD_TIL_CANCELLED);

    /**
     * @brief Cancel an existing order
     * @param order_id The ID of the order to cancel
     * @return true if cancellation successful, false otherwise
     */
    bool cancel_order(const std::string& order_id);

    /**
     * @brief Modify an existing order
     * @param order_id The ID of the order to modify
     * @param amount The new amount (optional)
     * @param price The new price (optional)
     * @return true if modification successful, false otherwise
     */
    bool modify_order(const std::string& order_id, 
                     double amount = 0.0, 
                     double price = 0.0);

    /**
     * @brief Get the current orderbook for an instrument
     * @param instrument_name The name of the instrument
     * @param depth The depth of the orderbook (default: 10)
     * @return The orderbook
     */
    OrderBook get_orderbook(const std::string& instrument_name, int depth = 10);

    /**
     * @brief Get all current positions
     * @return Vector of positions
     */
    std::vector<Position> get_positions();

    /**
     * @brief Get a specific position
     * @param instrument_name The name of the instrument
     * @return The position, or nullptr if no position exists
     */
    std::shared_ptr<Position> get_position(const std::string& instrument_name);

    /**
     * @brief Get all open orders
     * @return Vector of open orders
     */
    std::vector<Order> get_open_orders();

    /**
     * @brief Get order details
     * @param order_id The ID of the order
     * @return The order details, or nullptr if order not found
     */
    std::shared_ptr<Order> get_order(const std::string& order_id);

    /**
     * @brief Handle order updates from the API
     * @param update The order update JSON
     */
    void handle_order_update(const json& update);

    /**
     * @brief Handle position updates from the API
     * @param update The position update JSON
     */
    void handle_position_update(const json& update);

private:
    std::shared_ptr<ApiClient> api_client_;
    std::map<std::string, Order> open_orders_;
    std::map<std::string, Position> positions_;
    std::map<std::string, OrderBook> orderbooks_;
    std::mutex orders_mutex_;
    std::mutex positions_mutex_;
    std::mutex orderbooks_mutex_;

    // Helper methods
    std::string order_type_to_string(OrderType type);
    std::string order_direction_to_string(OrderDirection direction);
    std::string time_in_force_to_string(TimeInForce time_in_force);
};

} // namespace deribit

#endif // ORDER_MANAGER_H