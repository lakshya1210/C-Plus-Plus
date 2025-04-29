#ifndef DERIBIT_API_CLIENT_H
#define DERIBIT_API_CLIENT_H

#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include "websocketpp_asio_compatibility.h"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

namespace deribit {

using json = nlohmann::json;

/**
 * @enum InstrumentType
 * @brief Types of instruments available on Deribit
 */
enum class InstrumentType {
    SPOT,
    FUTURES,
    OPTIONS
};

/**
 * @struct ApiCredentials
 * @brief API credentials for authentication
 */
struct ApiCredentials {
    std::string api_key;
    std::string api_secret;
    std::string access_token;
    std::string refresh_token;
    std::chrono::system_clock::time_point token_expiry;
};

/**
 * @struct ApiResponse
 * @brief Response from the API
 */
struct ApiResponse {
    bool success;
    json data;
    std::string error_message;
};

/**
 * @class ApiClient
 * @brief Client for interacting with the Deribit API
 */
class ApiClient {
public:
    using WebSocketClient = websocketpp::client<websocketpp::config::asio_tls_client>;
    using WebSocketConnectionPtr = websocketpp::connection_hdl;
    using MessageCallback = std::function<void(const json&)>;
    
    /**
     * @brief Constructor
     * @param api_key The API key
     * @param api_secret The API secret
     * @param test_mode Whether to use the test environment (default: true)
     */
    ApiClient(const std::string& api_key, 
             const std::string& api_secret, 
             bool test_mode = true);
    
    /**
     * @brief Destructor
     */
    ~ApiClient();
    
    /**
     * @brief Initialize the API client
     * @return true if initialization successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Authenticate with the API
     * @return true if authentication successful, false otherwise
     */
    bool authenticate();
    
    /**
     * @brief Make a public API request
     * @param method The API method to call
     * @param params The parameters for the API call
     * @return The API response
     */
    ApiResponse public_request(const std::string& method, const json& params = {});
    
    /**
     * @brief Make a private API request (requires authentication)
     * @param method The API method to call
     * @param params The parameters for the API call
     * @return The API response
     */
    ApiResponse private_request(const std::string& method, const json& params = {});
    
    /**
     * @brief Connect to the WebSocket API
     * @return true if connection successful, false otherwise
     */
    bool connect_websocket();
    
    /**
     * @brief Disconnect from the WebSocket API
     */
    void disconnect_websocket();
    
    /**
     * @brief Subscribe to a channel
     * @param channel The channel to subscribe to
     * @param callback The callback to call when a message is received
     * @return true if subscription successful, false otherwise
     */
    bool subscribe(const std::string& channel, MessageCallback callback);
    
    /**
     * @brief Unsubscribe from a channel
     * @param channel The channel to unsubscribe from
     * @return true if unsubscription successful, false otherwise
     */
    bool unsubscribe(const std::string& channel);
    
    /**
     * @brief Get available instruments
     * @param currency The currency (e.g., "BTC")
     * @param type The instrument type
     * @return Vector of instrument names
     */
    std::vector<std::string> get_instruments(const std::string& currency, InstrumentType type);
    
    /**
     * @brief Check if the client is authenticated
     * @return true if authenticated, false otherwise
     */
    bool is_authenticated() const;
    
    /**
     * @brief Check if the WebSocket is connected
     * @return true if connected, false otherwise
     */
    bool is_websocket_connected() const;
    
    /**
     * @brief Get the API base URL
     * @return The API base URL
     */
    std::string get_api_url() const;
    
    /**
     * @brief Get the WebSocket URL
     * @return The WebSocket URL
     */
    std::string get_websocket_url() const;

private:
    // API configuration
    std::string api_key_;
    std::string api_secret_;
    bool test_mode_;
    std::string api_url_;
    std::string websocket_url_;
    
    // Authentication state
    ApiCredentials credentials_;
    std::mutex auth_mutex_;
    bool authenticated_;
    
    // WebSocket client
    std::unique_ptr<WebSocketClient> websocket_client_;
    WebSocketConnectionPtr websocket_connection_;
    std::mutex websocket_mutex_;
    bool websocket_connected_;
    std::thread websocket_thread_;
    std::map<std::string, MessageCallback> channel_callbacks_;
    
    // Message queue for asynchronous processing
    std::queue<std::pair<std::string, json>> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::thread message_thread_;
    bool running_;
    
    // Helper methods
    bool refresh_token();
    void websocket_message_handler(websocketpp::connection_hdl hdl, WebSocketClient::message_ptr msg);
    void process_message_queue();
    std::string create_signature(const std::string& method, const std::string& path, const std::string& nonce, const std::string& data);
    std::string instrument_type_to_string(InstrumentType type);
};

} // namespace deribit

#endif // DERIBIT_API_CLIENT_H