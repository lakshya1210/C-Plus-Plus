#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <string>
#include <map>
#include <set>
#include <mutex>
#include <memory>
#include <functional>
#include <thread>
#include "websocketpp_asio_compatibility.h"
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include "deribit_api_client.h"
#include "order_manager.h"

namespace deribit {

using json = nlohmann::json;

/**
 * @class WebSocketServer
 * @brief Server for distributing real-time market data to clients
 */
class WebSocketServer {
public:
    using WebSocketServerType = websocketpp::server<websocketpp::config::asio>;
    using ConnectionHandle = websocketpp::connection_hdl;
    using MessagePtr = WebSocketServerType::message_ptr;
    using ConnectionCallback = std::function<void(ConnectionHandle)>;
    using MessageCallback = std::function<void(ConnectionHandle, MessagePtr)>;
    
    /**
     * @brief Constructor
     * @param api_client Pointer to an initialized ApiClient
     * @param order_manager Pointer to an initialized OrderManager
     * @param port The port to listen on (default: 9000)
     */
    WebSocketServer(std::shared_ptr<ApiClient> api_client,
                   std::shared_ptr<OrderManager> order_manager,
                   uint16_t port = 9000);
    
    /**
     * @brief Destructor
     */
    ~WebSocketServer();
    
    /**
     * @brief Initialize the server
     * @return true if initialization successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Start the server
     * @return true if server started successfully, false otherwise
     */
    bool start();
    
    /**
     * @brief Stop the server
     */
    void stop();
    
    /**
     * @brief Check if the server is running
     * @return true if running, false otherwise
     */
    bool is_running() const;
    
    /**
     * @brief Set the connection open callback
     * @param callback The callback to call when a connection is opened
     */
    void set_open_callback(ConnectionCallback callback);
    
    /**
     * @brief Set the connection close callback
     * @param callback The callback to call when a connection is closed
     */
    void set_close_callback(ConnectionCallback callback);
    
    /**
     * @brief Set the message callback
     * @param callback The callback to call when a message is received
     */
    void set_message_callback(MessageCallback callback);
    
    /**
     * @brief Broadcast a message to all connected clients
     * @param message The message to broadcast
     */
    void broadcast(const std::string& message);
    
    /**
     * @brief Broadcast a message to subscribers of a specific channel
     * @param channel The channel to broadcast to
     * @param message The message to broadcast
     */
    void broadcast_to_channel(const std::string& channel, const std::string& message);
    
    /**
     * @brief Send a message to a specific client
     * @param hdl The connection handle of the client
     * @param message The message to send
     */
    void send(ConnectionHandle hdl, const std::string& message);
    
    /**
     * @brief Get the number of connected clients
     * @return The number of connected clients
     */
    size_t get_connection_count() const;
    
    /**
     * @brief Handle an orderbook update from the API
     * @param instrument_name The name of the instrument
     * @param orderbook The updated orderbook
     */
    void handle_orderbook_update(const std::string& instrument_name, const OrderBook& orderbook);

private:
    std::shared_ptr<ApiClient> api_client_;
    std::shared_ptr<OrderManager> order_manager_;
    uint16_t port_;
    std::unique_ptr<WebSocketServerType> server_;
    std::thread server_thread_;
    bool running_;
    
    // Connection management
    std::set<ConnectionHandle, std::owner_less<ConnectionHandle>> connections_;
    std::map<std::string, std::set<ConnectionHandle, std::owner_less<ConnectionHandle>>> channel_subscriptions_;
    std::map<ConnectionHandle, std::set<std::string>, std::owner_less<ConnectionHandle>> connection_subscriptions_;
    std::mutex connections_mutex_;
    std::mutex subscriptions_mutex_;
    
    // Callbacks
    ConnectionCallback open_callback_;
    ConnectionCallback close_callback_;
    MessageCallback message_callback_;
    
    // Event handlers
    void on_open(ConnectionHandle hdl);
    void on_close(ConnectionHandle hdl);
    void on_message(ConnectionHandle hdl, MessagePtr msg);
    
    // Subscription management
    bool subscribe_client(ConnectionHandle hdl, const std::string& channel);
    bool unsubscribe_client(ConnectionHandle hdl, const std::string& channel);
    void unsubscribe_all(ConnectionHandle hdl);
    
    // Message processing
    void process_message(ConnectionHandle hdl, const std::string& message);
    void handle_subscribe_request(ConnectionHandle hdl, const json& request);
    void handle_unsubscribe_request(ConnectionHandle hdl, const json& request);
};

} // namespace deribit

#endif // WEBSOCKET_SERVER_H