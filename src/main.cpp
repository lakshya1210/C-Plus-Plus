#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include "deribit_trading_system.h"

// Global trading system pointer for signal handling
deribit::TradingSystem* g_trading_system = nullptr;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    
    if (g_trading_system && g_trading_system->is_running()) {
        g_trading_system->stop();
    }
}

int main(int argc, char* argv[]) {
    try {
        // Check command line arguments
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " <api_key> <api_secret> [websocket_port]" << std::endl;
            return 1;
        }
        
        // Parse command line arguments
        std::string api_key = argv[1];
        std::string api_secret = argv[2];
        uint16_t websocket_port = (argc > 3) ? static_cast<uint16_t>(std::stoi(argv[3])) : 9000;
        
        // Set up signal handling
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
        
        // Create trading system
        deribit::TradingSystem trading_system(api_key, api_secret, true, websocket_port);
        g_trading_system = &trading_system;
        
        // Initialize trading system
        std::cout << "Initializing trading system..." << std::endl;
        if (!trading_system.initialize()) {
            std::cerr << "Failed to initialize trading system" << std::endl;
            return 1;
        }
        
        // Start trading system
        std::cout << "Starting trading system..." << std::endl;
        if (!trading_system.start()) {
            std::cerr << "Failed to start trading system" << std::endl;
            return 1;
        }
        
        // Subscribe to market data for BTC-PERPETUAL
        std::cout << "Subscribing to market data..." << std::endl;
        if (!trading_system.subscribe_market_data("BTC-PERPETUAL")) {
            std::cerr << "Failed to subscribe to market data" << std::endl;
        }
        
        // Get order manager
        auto order_manager = trading_system.get_order_manager();
        
        // Get orderbook
        std::cout << "Getting orderbook..." << std::endl;
        auto orderbook = order_manager->get_orderbook("BTC-PERPETUAL");
        
        // Print orderbook
        std::cout << "Orderbook for BTC-PERPETUAL:" << std::endl;
        std::cout << "Timestamp: " << orderbook.timestamp << std::endl;
        
        std::cout << "Bids:" << std::endl;
        for (size_t i = 0; i < std::min(orderbook.bids.size(), size_t(5)); ++i) {
            std::cout << "  " << orderbook.bids[i].first << " @ " << orderbook.bids[i].second << std::endl;
        }
        
        std::cout << "Asks:" << std::endl;
        for (size_t i = 0; i < std::min(orderbook.asks.size(), size_t(5)); ++i) {
            std::cout << "  " << orderbook.asks[i].first << " @ " << orderbook.asks[i].second << std::endl;
        }
        
        // Place a limit order (commented out to avoid actual trading)
        /*
        std::cout << "Placing limit order..." << std::endl;
        std::string order_id = order_manager->place_order(
            "BTC-PERPETUAL",
            deribit::OrderType::LIMIT,
            deribit::OrderDirection::BUY,
            0.1,  // amount
            orderbook.bids[0].first * 0.9,  // price 10% below best bid
            deribit::TimeInForce::GOOD_TIL_CANCELLED
        );
        
        if (!order_id.empty()) {
            std::cout << "Order placed with ID: " << order_id << std::endl;
            
            // Wait a bit
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // Modify the order
            std::cout << "Modifying order..." << std::endl;
            if (order_manager->modify_order(order_id, 0.2, 0.0)) {
                std::cout << "Order modified successfully" << std::endl;
            } else {
                std::cerr << "Failed to modify order" << std::endl;
            }
            
            // Wait a bit
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // Cancel the order
            std::cout << "Canceling order..." << std::endl;
            if (order_manager->cancel_order(order_id)) {
                std::cout << "Order canceled successfully" << std::endl;
            } else {
                std::cerr << "Failed to cancel order" << std::endl;
            }
        } else {
            std::cerr << "Failed to place order" << std::endl;
        }
        */
        
        // Run for a while to collect performance metrics
        std::cout << "Running for 30 seconds to collect performance metrics..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        // Print performance metrics
        std::cout << "\nPerformance Metrics:" << std::endl;
        trading_system.print_performance_metrics();
        
        // Export performance metrics to CSV
        std::string metrics_file = "performance_metrics.csv";
        if (trading_system.export_performance_metrics(metrics_file)) {
            std::cout << "Performance metrics exported to " << metrics_file << std::endl;
        } else {
            std::cerr << "Failed to export performance metrics" << std::endl;
        }
        
        // Unsubscribe from market data
        std::cout << "Unsubscribing from market data..." << std::endl;
        if (!trading_system.unsubscribe_market_data("BTC-PERPETUAL")) {
            std::cerr << "Failed to unsubscribe from market data" << std::endl;
        }
        
        // Stop trading system
        std::cout << "Stopping trading system..." << std::endl;
        trading_system.stop();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}