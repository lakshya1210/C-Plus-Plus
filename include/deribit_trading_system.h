#ifndef DERIBIT_TRADING_SYSTEM_H
#define DERIBIT_TRADING_SYSTEM_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include "deribit_api_client.h"
#include "order_manager.h"
#include "websocket_server.h"
#include "performance_monitor.h"

namespace deribit {

using json = nlohmann::json;

/**
 * @class TradingSystem
 * @brief Main class for the Deribit trading system
 */
class TradingSystem {
public:
    /**
     * @brief Constructor
     * @param api_key The API key
     * @param api_secret The API secret
     * @param test_mode Whether to use the test environment (default: true)
     * @param websocket_port The port for the WebSocket server (default: 9000)
     */
    TradingSystem(const std::string& api_key,
                 const std::string& api_secret,
                 bool test_mode = true,
                 uint16_t websocket_port = 9000);
    
    /**
     * @brief Destructor
     */
    ~TradingSystem();
    
    /**
     * @brief Initialize the trading system
     * @return true if initialization successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Start the trading system
     * @return true if system started successfully, false otherwise
     */
    bool start();
    
    /**
     * @brief Stop the trading system
     */
    void stop();
    
    /**
     * @brief Check if the system is running
     * @return true if running, false otherwise
     */
    bool is_running() const;
    
    /**
     * @brief Get the API client
     * @return Shared pointer to the API client
     */
    std::shared_ptr<ApiClient> get_api_client() const;
    
    /**
     * @brief Get the order manager
     * @return Shared pointer to the order manager
     */
    std::shared_ptr<OrderManager> get_order_manager() const;
    
    /**
     * @brief Get the WebSocket server
     * @return Shared pointer to the WebSocket server
     */
    std::shared_ptr<WebSocketServer> get_websocket_server() const;
    
    /**
     * @brief Wait for the system to stop
     */
    void wait();
    
    /**
     * @brief Subscribe to market data for an instrument
     * @param instrument_name The name of the instrument
     * @return true if subscription successful, false otherwise
     */
    bool subscribe_market_data(const std::string& instrument_name);
    
    /**
     * @brief Unsubscribe from market data for an instrument
     * @param instrument_name The name of the instrument
     * @return true if unsubscription successful, false otherwise
     */
    bool unsubscribe_market_data(const std::string& instrument_name);
    
    /**
     * @brief Get performance metrics
     * @return Map of operation names to latency metrics
     */
    std::map<std::string, LatencyMetric> get_performance_metrics() const;
    
    /**
     * @brief Export performance metrics to a CSV file
     * @param filename The name of the file to export to
     * @return true if export successful, false otherwise
     */
    bool export_performance_metrics(const std::string& filename) const;
    
    /**
     * @brief Print performance metrics to the console
     */
    void print_performance_metrics() const;

private:
    std::string api_key_;
    std::string api_secret_;
    bool test_mode_;
    uint16_t websocket_port_;
    
    std::shared_ptr<ApiClient> api_client_;
    std::shared_ptr<OrderManager> order_manager_;
    std::shared_ptr<WebSocketServer> websocket_server_;
    
    std::atomic<bool> running_{false};
    std::mutex wait_mutex_;
    std::condition_variable wait_condition_;
    
    // Market data handling
    void handle_orderbook_update(const json& update);
    void handle_trade_update(const json& update);
    void handle_instrument_update(const json& update);
    
    // Order and position handling
    void handle_order_update(const json& update);
    void handle_position_update(const json& update);
};

} // namespace deribit

#endif // DERIBIT_TRADING_SYSTEM_H