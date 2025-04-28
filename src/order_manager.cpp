#include "order_manager.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include "performance_monitor.h"

namespace deribit {

OrderManager::OrderManager(std::shared_ptr<ApiClient> api_client)
    : api_client_(api_client) {
    // Validate API client
    if (!api_client_) {
        throw std::invalid_argument("API client cannot be null");
    }
}

std::string OrderManager::place_order(const std::string& instrument_name,
                                     OrderType type,
                                     OrderDirection direction,
                                     double amount,
                                     double price,
                                     TimeInForce time_in_force) {
    // Start latency tracking
    auto tracker = PerformanceMonitor::instance().get_tracker("place_order", true);
    auto tracking_id = tracker->start();
    
    try {
        // Validate parameters
        if (instrument_name.empty()) {
            throw std::invalid_argument("Instrument name cannot be empty");
        }
        
        if (amount <= 0.0) {
            throw std::invalid_argument("Amount must be positive");
        }
        
        if (type == OrderType::LIMIT && price <= 0.0) {
            throw std::invalid_argument("Price must be positive for limit orders");
        }
        
        // Create request parameters
        json params = {
            {"instrument_name", instrument_name},
            {"amount", amount},
            {"type", order_type_to_string(type)},
            {"label", "deribit_trading_system"}
        };
        
        // Add direction-specific parameters
        if (direction == OrderDirection::BUY) {
            params["side"] = "buy";
        } else {
            params["side"] = "sell";
        }
        
        // Add type-specific parameters
        if (type == OrderType::LIMIT || type == OrderType::STOP_LIMIT) {
            params["price"] = price;
            params["time_in_force"] = time_in_force_to_string(time_in_force);
        }
        
        // Make API request
        ApiResponse response = api_client_->private_request("private/buy", params);
        
        if (response.success) {
            // Extract order ID
            std::string order_id = response.data["result"]["order"]["order_id"];
            
            // Store order in cache
            Order order;
            order.order_id = order_id;
            order.instrument_name = instrument_name;
            order.type = type;
            order.direction = direction;
            order.price = price;
            order.amount = amount;
            order.time_in_force = time_in_force;
            order.status = "open";
            order.created_at = response.data["result"]["order"]["creation_timestamp"];
            order.last_updated_at = order.created_at;
            
            // Add to open orders
            std::lock_guard<std::mutex> lock(orders_mutex_);
            open_orders_[order_id] = order;
            
            // End latency tracking
            tracker->end(tracking_id);
            
            return order_id;
        } else {
            std::cerr << "Error placing order: " << response.error_message << std::endl;
            
            // End latency tracking
            tracker->end(tracking_id);
            
            return "";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error placing order: " << e.what() << std::endl;
        
        // End latency tracking
        tracker->end(tracking_id);
        
        return "";
    }
}

bool OrderManager::cancel_order(const std::string& order_id) {
    // Start latency tracking
    auto tracker = PerformanceMonitor::instance().get_tracker("cancel_order", true);
    auto tracking_id = tracker->start();
    
    try {
        // Validate parameters
        if (order_id.empty()) {
            throw std::invalid_argument("Order ID cannot be empty");
        }
        
        // Create request parameters
        json params = {
            {"order_id", order_id}
        };
        
        // Make API request
        ApiResponse response = api_client_->private_request("private/cancel", params);
        
        if (response.success) {
            // Remove from open orders
            std::lock_guard<std::mutex> lock(orders_mutex_);
            open_orders_.erase(order_id);
            
            // End latency tracking
            tracker->end(tracking_id);
            
            return true;
        } else {
            std::cerr << "Error canceling order: " << response.error_message << std::endl;
            
            // End latency tracking
            tracker->end(tracking_id);
            
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error canceling order: " << e.what() << std::endl;
        
        // End latency tracking
        tracker->end(tracking_id);
        
        return false;
    }
}

bool OrderManager::modify_order(const std::string& order_id, double amount, double price) {
    // Start latency tracking
    auto tracker = PerformanceMonitor::instance().get_tracker("modify_order", true);
    auto tracking_id = tracker->start();
    
    try {
        // Validate parameters
        if (order_id.empty()) {
            throw std::invalid_argument("Order ID cannot be empty");
        }
        
        if (amount <= 0.0 && price <= 0.0) {
            throw std::invalid_argument("Either amount or price must be specified");
        }
        
        // Create request parameters
        json params = {
            {"order_id", order_id}
        };
        
        if (amount > 0.0) {
            params["amount"] = amount;
        }
        
        if (price > 0.0) {
            params["price"] = price;
        }
        
        // Make API request
        ApiResponse response = api_client_->private_request("private/edit", params);
        
        if (response.success) {
            // Update order in cache
            std::lock_guard<std::mutex> lock(orders_mutex_);
            auto it = open_orders_.find(order_id);
            
            if (it != open_orders_.end()) {
                if (amount > 0.0) {
                    it->second.amount = amount;
                }
                
                if (price > 0.0) {
                    it->second.price = price;
                }
                
                it->second.last_updated_at = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            }
            
            // End latency tracking
            tracker->end(tracking_id);
            
            return true;
        } else {
            std::cerr << "Error modifying order: " << response.error_message << std::endl;
            
            // End latency tracking
            tracker->end(tracking_id);
            
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error modifying order: " << e.what() << std::endl;
        
        // End latency tracking
        tracker->end(tracking_id);
        
        return false;
    }
}

OrderBook OrderManager::get_orderbook(const std::string& instrument_name, int depth) {
    // Start latency tracking
    auto tracker = PerformanceMonitor::instance().get_tracker("get_orderbook", true);
    auto tracking_id = tracker->start();
    
    OrderBook orderbook;
    orderbook.instrument_name = instrument_name;
    
    try {
        // Validate parameters
        if (instrument_name.empty()) {
            throw std::invalid_argument("Instrument name cannot be empty");
        }
        
        if (depth <= 0) {
            throw std::invalid_argument("Depth must be positive");
        }
        
        // Check cache first
        {
            std::lock_guard<std::mutex> lock(orderbooks_mutex_);
            auto it = orderbooks_.find(instrument_name);
            
            if (it != orderbooks_.end()) {
                // End latency tracking
                tracker->end(tracking_id);
                
                return it->second;
            }
        }
        
        // Create request parameters
        json params = {
            {"instrument_name", instrument_name},
            {"depth", depth}
        };
        
        // Make API request
        ApiResponse response = api_client_->public_request("public/get_order_book", params);
        
        if (response.success) {
            // Parse orderbook
            orderbook.timestamp = response.data["result"]["timestamp"];
            
            // Parse bids
            for (const auto& bid : response.data["result"]["bids"]) {
                double price = bid[0];
                double size = bid[1];
                orderbook.bids.push_back(std::make_pair(price, size));
            }
            
            // Parse asks
            for (const auto& ask : response.data["result"]["asks"]) {
                double price = ask[0];
                double size = ask[1];
                orderbook.asks.push_back(std::make_pair(price, size));
            }
            
            // Store in cache
            std::lock_guard<std::mutex> lock(orderbooks_mutex_);
            orderbooks_[instrument_name] = orderbook;
        } else {
            std::cerr << "Error getting orderbook: " << response.error_message << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting orderbook: " << e.what() << std::endl;
    }
    
    // End latency tracking
    tracker->end(tracking_id);
    
    return orderbook;
}

std::vector<Position> OrderManager::get_positions() {
    std::vector<Position> result;
    
    try {
        // Make API request
        ApiResponse response = api_client_->private_request("private/get_positions", {});
        
        if (response.success) {
            // Parse positions
            for (const auto& pos : response.data["result"]) {
                Position position;
                position.instrument_name = pos["instrument_name"];
                position.size = pos["size"];
                position.entry_price = pos["average_price"];
                position.mark_price = pos["mark_price"];
                position.liquidation_price = pos["estimated_liquidation_price"];
                position.unrealized_pnl = pos["floating_profit_loss"];
                position.realized_pnl = pos["realized_profit_loss"];
                
                result.push_back(position);
                
                // Update cache
                std::lock_guard<std::mutex> lock(positions_mutex_);
                positions_[position.instrument_name] = position;
            }
        } else {
            std::cerr << "Error getting positions: " << response.error_message << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting positions: " << e.what() << std::endl;
    }
    
    return result;
}

std::shared_ptr<Position> OrderManager::get_position(const std::string& instrument_name) {
    try {
        // Validate parameters
        if (instrument_name.empty()) {
            throw std::invalid_argument("Instrument name cannot be empty");
        }
        
        // Check cache first
        {
            std::lock_guard<std::mutex> lock(positions_mutex_);
            auto it = positions_.find(instrument_name);
            
            if (it != positions_.end()) {
                return std::make_shared<Position>(it->second);
            }
        }
        
        // Create request parameters
        json params = {
            {"instrument_name", instrument_name}
        };
        
        // Make API request
        ApiResponse response = api_client_->private_request("private/get_position", params);
        
        if (response.success) {
            // Parse position
            auto pos = std::make_shared<Position>();
            pos->instrument_name = response.data["result"]["instrument_name"];
            pos->size = response.data["result"]["size"];
            pos->entry_price = response.data["result"]["average_price"];
            pos->mark_price = response.data["result"]["mark_price"];
            pos->liquidation_price = response.data["result"]["estimated_liquidation_price"];
            pos->unrealized_pnl = response.data["result"]["floating_profit_loss"];
            pos->realized_pnl = response.data["result"]["realized_profit_loss"];
            
            // Update cache
            std::lock_guard<std::mutex> lock(positions_mutex_);
            positions_[instrument_name] = *pos;
            
            return pos;
        } else {
            std::cerr << "Error getting position: " << response.error_message << std::endl;
            return nullptr;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting position: " << e.what() << std::endl;
        return nullptr;
    }
}

std::vector<Order> OrderManager::get_open_orders() {
    std::vector<Order> result;
    
    try {
        // Make API request
        ApiResponse response = api_client_->private_request("private/get_open_orders_by_currency", {});
        
        if (response.success) {
            // Parse orders
            for (const auto& order_json : response.data["result"]) {
                Order order;
                order.order_id = order_json["order_id"];
                order.instrument_name = order_json["instrument_name"];
                
                // Parse order type
                std::string type_str = order_json["order_type"];
                if (type_str == "limit") {
                    order.type = OrderType::LIMIT;
                } else if (type_str == "market") {
                    order.type = OrderType::MARKET;
                } else if (type_str == "stop_market") {
                    order.type = OrderType::STOP_MARKET;
                } else if (type_str == "stop_limit") {
                    order.type = OrderType::STOP_LIMIT;
                }
                
                // Parse direction
                std::string direction_str = order_json["direction"];
                if (direction_str == "buy") {
                    order.direction = OrderDirection::BUY;
                } else {
                    order.direction = OrderDirection::SELL;
                }
                
                order.price = order_json["price"];
                order.amount = order_json["amount"];
                
                // Parse time in force
                std::string tif_str = order_json["time_in_force"];
                if (tif_str == "good_til_cancelled") {
                    order.time_in_force = TimeInForce::GOOD_TIL_CANCELLED;
                } else if (tif_str == "fill_or_kill") {
                    order.time_in_force = TimeInForce::FILL_OR_KILL;
                } else if (tif_str == "immediate_or_cancel") {
                    order.time_in_force = TimeInForce::IMMEDIATE_OR_CANCEL;
                }
                
                order.status = order_json["order_state"];
                order.created_at = order_json["creation_timestamp"];
                order.last_updated_at = order_json["last_update_timestamp"];
                
                result.push_back(order);
                
                // Update cache
                std::lock_guard<std::mutex> lock(orders_mutex_);
                open_orders_[order.order_id] = order;
            }
        } else {
            std::cerr << "Error getting open orders: " << response.error_message << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting open orders: " << e.what() << std::endl;
    }
    
    return result;
}

std::shared_ptr<Order> OrderManager::get_order(const std::string& order_id) {
    try {
        // Validate parameters
        if (order_id.empty()) {
            throw std::invalid_argument("Order ID cannot be empty");
        }
        
        // Check cache first
        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            auto it = open_orders_.find(order_id);
            
            if (it != open_orders_.end()) {
                return std::make_shared<Order>(it->second);
            }
        }
        
        // Create request parameters
        json params = {
            {"order_id", order_id}
        };
        
        // Make API request
        ApiResponse response = api_client_->private_request("private/get_order_state", params);
        
        if (response.success) {
            // Parse order
            auto order = std::make_shared<Order>();
            order->order_id = response.data["result"]["order_id"];
            order->instrument_name = response.data["result"]["instrument_name"];
            
            // Parse order type
            std::string type_str = response.data["result"]["order_type"];
            if (type_str == "limit") {
                order->type = OrderType::LIMIT;
            } else if (type_str == "market") {
                order->type = OrderType::MARKET;
            } else if (type_str == "stop_market") {
                order->type = OrderType::STOP_MARKET;
            } else if (type_str == "stop_limit") {
                order->type = OrderType::STOP_LIMIT;
            }
            
            // Parse direction
            std::string direction_str = response.data["result"]["direction"];
            if (direction_str == "buy") {
                order->direction = OrderDirection::BUY;
            } else {
                order->direction = OrderDirection::SELL;
            }
            
            order->price = response.data["result"]["price"];
            order->amount = response.data["result"]["amount"];
            
            // Parse time in force
            std::string tif_str = response.data["result"]["time_in_force"];
            if (tif_str == "good_til_cancelled") {
                order->time_in_force = TimeInForce::GOOD_TIL_CANCELLED;
            } else if (tif_str == "fill_or_kill") {
                order->time_in_force = TimeInForce::FILL_OR_KILL;
            } else if (tif_str == "immediate_or_cancel") {
                order->time_in_force = TimeInForce::IMMEDIATE_OR_CANCEL;
            }
            
            order->status = response.data["result"]["order_state"];
            order->created_at = response.data["result"]["creation_timestamp"];
            order->last_updated_at = response.data["result"]["last_update_timestamp"];
            
            // Update cache if order is still open
            if (order->status == "open") {
                std::lock_guard<std::mutex> lock(orders_mutex_);
                open_orders_[order_id] = *order;
            }
            
            return order;
        } else {
            std::cerr << "Error getting order: " << response.error_message << std::endl;
            return nullptr;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting order: " << e.what() << std::endl;
        return nullptr;
    }
}

void OrderManager::handle_order_update(const json& update) {
    try {
        // Extract order ID
        std::string order_id = update["order_id"];
        
        // Extract order status
        std::string status = update["order_state"];
        
        // Update or remove from cache based on status
        std::lock_guard<std::mutex> lock(orders_mutex_);
        
        if (status == "open" || status == "untriggered") {
            // Update existing order or add new one
            Order order;
            order.order_id = order_id;
            order.instrument_name = update["instrument_name"];
            
            // Parse order type
            std::string type_str = update["order_type"];
            if (type_str == "limit") {
                order.type = OrderType::LIMIT;
            } else if (type_str == "market") {
                order.type = OrderType::MARKET;
            } else if (type_str == "stop_market") {
                order.type = OrderType::STOP_MARKET;
            } else if (type_str == "stop_limit") {
                order.type = OrderType::STOP_LIMIT;
            }
            
            // Parse direction
            std::string direction_str = update["direction"];
            if (direction_str == "buy") {
                order.direction = OrderDirection::BUY;
            } else {
                order.direction = OrderDirection::SELL;
            }
            
            order.price = update["price"];
            order.amount = update["amount"];
            
            // Parse time in force
            std::string tif_str = update["time_in_force"];
            if (tif_str == "good_til_cancelled") {
                order.time_in_force = TimeInForce::GOOD_TIL_CANCELLED;
            } else if (tif_str == "fill_or_kill") {
                order.time_in_force = TimeInForce::FILL_OR_KILL;
            } else if (tif_str == "immediate_or_cancel") {
                order.time_in_force = TimeInForce::IMMEDIATE_OR_CANCEL;
            }
            
            order.status = status;
            order.created_at = update["creation_timestamp"];
            order.last_updated_at = update["last_update_timestamp"];
            
            open_orders_[order_id] = order;
        } else {
            // Order is no longer open, remove from cache
            open_orders_.erase(order_id);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling order update: " << e.what() << std::endl;
    }
}

void OrderManager::handle_position_update(const json& update) {
    try {
        // Extract instrument name
        std::string instrument_name = update["instrument_name"];
        
        // Update position in cache
        Position position;
        position.instrument_name = instrument_name;
        position.size = update["size"];
        position.entry_price = update["average_price"];
        position.mark_price = update["mark_price"];
        position.liquidation_price = update["estimated_liquidation_price"];
        position.unrealized_pnl = update["floating_profit_loss"];
        position.realized_pnl = update["realized_profit_loss"];
        
        std::lock_guard<std::mutex> lock(positions_mutex_);
        positions_[instrument_name] = position;
    } catch (const std::exception& e) {
        std::cerr << "Error handling position update: " << e.what() << std::endl;
    }
}

std::string OrderManager::order_type_to_string(OrderType type) {
    switch (type) {
        case OrderType::MARKET:
            return "market";
        case OrderType::LIMIT:
            return "limit";
        case OrderType::STOP_MARKET:
            return "stop_market";
        case OrderType::STOP_LIMIT:
            return "stop_limit";
        default:
            return "";
    }
}

std::string OrderManager::order_direction_to_string(OrderDirection direction) {
    switch (direction) {
        case OrderDirection::BUY:
            return "buy";
        case OrderDirection::SELL:
            return "sell";
        default:
            return "";
    }
}

std::string OrderManager::time_in_force_to_string(TimeInForce time_in_force) {
    switch (time_in_force) {
        case TimeInForce::GOOD_TIL_CANCELLED:
            return "good_til_cancelled";
        case TimeInForce::FILL_OR_KILL:
            return "fill_or_kill";
        case TimeInForce::IMMEDIATE_OR_CANCEL:
            return "immediate_or_cancel";
        default:
            return "";
    }
}

} // namespace deribit