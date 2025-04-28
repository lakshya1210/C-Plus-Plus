#include "deribit_trading_system.h"
#include <iostream>
#include <chrono>
#include <thread>

namespace deribit {

TradingSystem::TradingSystem(const std::string& api_key,
                           const std::string& api_secret,
                           bool test_mode,
                           uint16_t websocket_port)
    : api_key_(api_key),
      api_secret_(api_secret),
      test_mode_(test_mode),
      websocket_port_(websocket_port) {
}

TradingSystem::~TradingSystem() {
    // Stop the system if running
    if (running_) {
        stop();
    }
}

bool TradingSystem::initialize() {
    try {
        // Initialize API client
        api_client_ = std::make_shared<ApiClient>(api_key_, api_secret_, test_mode_);
        if (!api_client_->initialize()) {
            std::cerr << "Failed to initialize API client" << std::endl;
            return false;
        }
        
        // Authenticate with API
        if (!api_client_->authenticate()) {
            std::cerr << "Failed to authenticate with API" << std::endl;
            return false;
        }
        
        // Initialize order manager
        order_manager_ = std::make_shared<OrderManager>(api_client_);
        
        // Initialize WebSocket server
        websocket_server_ = std::make_shared<WebSocketServer>(api_client_, order_manager_, websocket_port_);
        if (!websocket_server_->initialize()) {
            std::cerr << "Failed to initialize WebSocket server" << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing trading system: " << e.what() << std::endl;
        return false;
    }
}

bool TradingSystem::start() {
    if (running_) {
        return true;
    }
    
    try {
        // Connect to WebSocket API
        if (!api_client_->connect_websocket()) {
            std::cerr << "Failed to connect to WebSocket API" << std::endl;
            return false;
        }
        
        // Start WebSocket server
        if (!websocket_server_->start()) {
            std::cerr << "Failed to start WebSocket server" << std::endl;
            return false;
        }
        
        running_ = true;
        
        std::cout << "Trading system started" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error starting trading system: " << e.what() << std::endl;
        return false;
    }
}

void TradingSystem::stop() {
    if (!running_) {
        return;
    }
    
    try {
        // Disconnect from WebSocket API
        api_client_->disconnect_websocket();
        
        // Stop WebSocket server
        websocket_server_->stop();
        
        running_ = false;
        
        // Notify waiting threads
        wait_condition_.notify_all();
        
        std::cout << "Trading system stopped" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error stopping trading system: " << e.what() << std::endl;
    }
}

bool TradingSystem::is_running() const {
    return running_;
}

std::shared_ptr<ApiClient> TradingSystem::get_api_client() const {
    return api_client_;
}

std::shared_ptr<OrderManager> TradingSystem::get_order_manager() const {
    return order_manager_;
}

std::shared_ptr<WebSocketServer> TradingSystem::get_websocket_server() const {
    return websocket_server_;
}

void TradingSystem::wait() {
    std::unique_lock<std::mutex> lock(wait_mutex_);
    wait_condition_.wait(lock, [this] { return !running_; });
}

bool TradingSystem::subscribe_market_data(const std::string& instrument_name) {
    if (!running_) {
        std::cerr << "Cannot subscribe: system not running" << std::endl;
        return false;
    }
    
    try {
        // Subscribe to orderbook channel
        std::string channel = "book." + instrument_name + ".100ms";
        
        return api_client_->subscribe(channel, [this, instrument_name](const json& data) {
            handle_orderbook_update(data);
        });
    } catch (const std::exception& e) {
        std::cerr << "Error subscribing to market data: " << e.what() << std::endl;
        return false;
    }
}

bool TradingSystem::unsubscribe_market_data(const std::string& instrument_name) {
    if (!running_) {
        std::cerr << "Cannot unsubscribe: system not running" << std::endl;
        return false;
    }
    
    try {
        // Unsubscribe from orderbook channel
        std::string channel = "book." + instrument_name + ".100ms";
        
        return api_client_->unsubscribe(channel);
    } catch (const std::exception& e) {
        std::cerr << "Error unsubscribing from market data: " << e.what() << std::endl;
        return false;
    }
}

std::map<std::string, LatencyMetric> TradingSystem::get_performance_metrics() const {
    return PerformanceMonitor::instance().get_all_metrics();
}

bool TradingSystem::export_performance_metrics(const std::string& filename) const {
    return PerformanceMonitor::instance().export_to_csv(filename);
}

void TradingSystem::print_performance_metrics() const {
    PerformanceMonitor::instance().print_metrics();
}

void TradingSystem::handle_orderbook_update(const json& update) {
    try {
        // Start latency tracking
        auto tracker = PerformanceMonitor::instance().get_tracker("process_orderbook_update", true);
        auto tracking_id = tracker->start();
        
        // Extract instrument name
        std::string instrument_name = update["instrument_name"];
        
        // Create orderbook
        OrderBook orderbook;
        orderbook.instrument_name = instrument_name;
        orderbook.timestamp = update["timestamp"];
        
        // Parse bids
        for (const auto& bid : update["bids"]) {
            double price = bid[0];
            double size = bid[1];
            orderbook.bids.push_back(std::make_pair(price, size));
        }
        
        // Parse asks
        for (const auto& ask : update["asks"]) {
            double price = ask[0];
            double size = ask[1];
            orderbook.asks.push_back(std::make_pair(price, size));
        }
        
        // Update WebSocket server
        websocket_server_->handle_orderbook_update(instrument_name, orderbook);
        
        // End latency tracking
        tracker->end(tracking_id);
    } catch (const std::exception& e) {
        std::cerr << "Error handling orderbook update: " << e.what() << std::endl;
    }
}

void TradingSystem::handle_trade_update(const json& update) {
    // Not implemented yet
}

void TradingSystem::handle_instrument_update(const json& update) {
    // Not implemented yet
}

void TradingSystem::handle_order_update(const json& update) {
    try {
        // Forward to order manager
        order_manager_->handle_order_update(update);
    } catch (const std::exception& e) {
        std::cerr << "Error handling order update: " << e.what() << std::endl;
    }
}

void TradingSystem::handle_position_update(const json& update) {
    try {
        // Forward to order manager
        order_manager_->handle_position_update(update);
    } catch (const std::exception& e) {
        std::cerr << "Error handling position update: " << e.what() << std::endl;
    }
}

} // namespace deribit