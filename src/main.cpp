#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include "deribit_trading_system.h"
using namespace std;
// Global trading system pointer for signal handling
deribit::TradingSystem* g_trading_system = nullptr;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    cout << "Received signal " << signal << ", shutting down..." << std;
    
    if (g_trading_system && g_trading_system->is_running()) {
        g_trading_system->stop();
    }
}

int main(int argc, char* argv[]) {
    try {
        // Check command line arguments
        if (argc < 3) {
            cerr << "Usage: " << argv[0] << " <api_key> <api_secret> [websocket_port]" << endl;
            return 1;
        }
        
        // Parse command line arguments
        string api_key = argv[1];
        string api_secret = argv[2];
        uint16_t websocket_port = (argc > 3) ? static_cast<uint16_t>(stoi(argv[3])) : 9000;
        
        // Set up signal handling
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // Create trading system
        deribit::TradingSystem trading_system(api_key, api_secret, true, websocket_port);
        g_trading_system = &trading_system;
        
        // Initialize trading system
        cout << "Initializing trading system..." << endl;
        if (!trading_system.initialize()) {
            cerr << "Failed to initialize trading system" << endl;
            return 1;
        }
        
        // Start trading system
        cout << "Starting trading system..." << endl;
        if (!trading_system.start()) {
            cerr << "Failed to start trading system" << endl;
            return 1;
        }
        
        // Subscribe to market data for BTC-PERPETUAL
        cout << "Subscribing to market data..." << endl;
        if (!trading_system.subscribe_market_data("BTC-PERPETUAL")) {
            cerr << "Failed to subscribe to market data" << endl;
        }
        
        // Get order manager
        auto order_manager = trading_system.get_order_manager();
        
        // Get orderbook
        cout << "Getting orderbook..." << endl;
        auto orderbook = order_manager->get_orderbook("BTC-PERPETUAL");
        
        // Print orderbook
        cout << "Orderbook for BTC-PERPETUAL:" << endl;
        cout << "Timestamp: " << orderbook.timestamp << endl;
        
        cout << "Bids:" << endl;
        for (size_t i = 0; i < min(orderbook.bids.size(), size_t(5)); ++i) {
            cout << "  " << orderbook.bids[i].first << " @ " << orderbook.bids[i].second << endl;
        }
        
        cout << "Asks:" << endl;
        for (size_t i = 0; i < min(orderbook.asks.size(), size_t(5)); ++i) {
            cout << "  " << orderbook.asks[i].first << " @ " << orderbook.asks[i].second << endl;
        }
        
        // Place a limit order (commented out to avoid actual trading)
        /*
        cout << "Placing limit order..." << endl;
        string order_id = order_manager->place_order(
            "BTC-PERPETUAL",
            deribit::OrderType::LIMIT,
            deribit::OrderDirection::BUY,
            0.1,  // amount
            orderbook.bids[0].first * 0.9,  // price 10% below best bid
            deribit::TimeInForce::GOOD_TIL_CANCELLED
        );
        
        if (!order_id.empty()) {
            cout << "Order placed with ID: " << order_id << endl;
            
            // Wait a bit
            this_thread::sleep_for(chrono::seconds(2));
            
            // Modify the order
            cout << "Modifying order..." << endl;
            if (order_manager->modify_order(order_id, 0.2, 0.0)) {
                cout << "Order modified successfully" << endl;
            } else {
                cerr << "Failed to modify order" << endl;
            }
            
            // Wait a bit
            this_thread::sleep_for(chrono::seconds(2));
            
            // Cancel the order
            cout << "Canceling order..." << endl;
            if (order_manager->cancel_order(order_id)) {
                cout << "Order canceled successfully" << endl;
            } else {
                cerr << "Failed to cancel order" << endl;
            }
        } else {
            cerr << "Failed to place order" << endl;
        }
        */
        
        // Run for a while to collect performance metrics
        cout << "Running for 30 seconds to collect performance metrics..." << endl;
        this_thread::sleep_for(chrono::seconds(30));
        
        // Print performance metrics
        cout << "\nPerformance Metrics:" << endl;
        trading_system.print_performance_metrics();
        
        // Export performance metrics to CSV
        string metrics_file = "performance_metrics.csv";
        if (trading_system.export_performance_metrics(metrics_file)) {
            cout << "Performance metrics exported to " << metrics_file << endl;
        } else {
            cerr << "Failed to export performance metrics" << endl;
        }
        
        // Unsubscribe from market data
        cout << "Unsubscribing from market data..." << endl;
        if (!trading_system.unsubscribe_market_data("BTC-PERPETUAL")) {
            cerr << "Failed to unsubscribe from market data" << endl;
        }
        
        // Stop trading system
        cout << "Stopping trading system..." << endl;
        trading_system.stop();
        
        return 0;
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}