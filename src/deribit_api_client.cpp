#include "deribit_api_client.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>

namespace deribit {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

ApiClient::ApiClient(const std::string& api_key, const std::string& api_secret, bool test_mode)
    : api_key_(api_key),
      api_secret_(api_secret),
      test_mode_(test_mode),
      authenticated_(false),
      websocket_connected_(false),
      running_(false) {
    
    // Set API URLs based on test mode
    if (test_mode_) {
        api_url_ = "https://test.deribit.com";
        websocket_url_ = "wss://test.deribit.com/ws/api/v2";
    } else {
        api_url_ = "https://www.deribit.com";
        websocket_url_ = "wss://www.deribit.com/ws/api/v2";
    }
}

ApiClient::~ApiClient() {
    // Stop message processing thread
    running_ = false;
    if (message_thread_.joinable()) {
        queue_condition_.notify_all();
        message_thread_.join();
    }
    
    // Disconnect WebSocket
    disconnect_websocket();
}

bool ApiClient::initialize() {
    try {
        // Initialize WebSocket client
        websocket_client_ = std::make_unique<WebSocketClient>();
        
        // Set WebSocket client options
        websocket_client_->set_access_channels(websocketpp::log::alevel::none);
        websocket_client_->set_error_channels(websocketpp::log::elevel::fatal);
        websocket_client_->init_asio();
        websocket_client_->set_tls_init_handler([](websocketpp::connection_hdl) {
            return websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        });
        
        // Start message processing thread
        running_ = true;
        message_thread_ = std::thread(&ApiClient::process_message_queue, this);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing API client: " << e.what() << std::endl;
        return false;
    }
}

bool ApiClient::authenticate() {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    try {
        // Create authentication request
        json params = {
            {"grant_type", "client_credentials"},
            {"client_id", api_key_},
            {"client_secret", api_secret_}
        };
        
        // Make authentication request
        ApiResponse response = public_request("public/auth", params);
        
        if (response.success) {
            // Store authentication tokens
            credentials_.api_key = api_key_;
            credentials_.api_secret = api_secret_;
            credentials_.access_token = response.data["result"]["access_token"];
            credentials_.refresh_token = response.data["result"]["refresh_token"];
            
            // Calculate token expiry time
            int expires_in = response.data["result"]["expires_in"];
            credentials_.token_expiry = std::chrono::system_clock::now() + std::chrono::seconds(expires_in);
            
            authenticated_ = true;
            return true;
        } else {
            std::cerr << "Authentication failed: " << response.error_message << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during authentication: " << e.what() << std::endl;
        return false;
    }
}

ApiResponse ApiClient::public_request(const std::string& method, const json& params) {
    try {
        // Parse URL
        std::string host = api_url_.substr(8); // Remove "https://"
        std::string target = "/api/v2/" + method;
        
        // Create request body
        json request_body = {
            {"jsonrpc", "2.0"},
            {"id", 42},
            {"method", method},
            {"params", params}
        };
        
        std::string body = request_body.dump();
        
        // Set up SSL context
        ssl::context ctx(ssl::context::tlsv12_client);
        ctx.set_default_verify_paths();
        
        // Create and connect socket
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
        
        // Set SNI hostname (required for SSL)
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            throw beast::system_error{ec};
        }
        
        // Resolve and connect
        auto const results = resolver.resolve(host, "443");
        beast::get_lowest_layer(stream).connect(results);
        
        // Perform SSL handshake
        stream.handshake(ssl::stream_base::client);
        
        // Create HTTP request
        http::request<http::string_body> req{http::verb::post, target, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "DeribitTradingSystem/1.0");
        req.set(http::field::content_type, "application/json");
        req.set(http::field::accept, "application/json");
        req.body() = body;
        req.prepare_payload();
        
        // Send HTTP request
        http::write(stream, req);
        
        // Receive HTTP response
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        
        // Close connection
        beast::error_code ec;
        stream.shutdown(ec);
        
        // Parse response
        json response_json = json::parse(res.body());
        
        ApiResponse api_response;
        if (response_json.contains("error")) {
            api_response.success = false;
            api_response.error_message = response_json["error"]["message"];
        } else {
            api_response.success = true;
            api_response.data = response_json;
        }
        
        return api_response;
    } catch (const std::exception& e) {
        ApiResponse api_response;
        api_response.success = false;
        api_response.error_message = std::string("Request failed: ") + e.what();
        return api_response;
    }
}

ApiResponse ApiClient::private_request(const std::string& method, const json& params) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    // Check if authenticated
    if (!authenticated_) {
        ApiResponse response;
        response.success = false;
        response.error_message = "Not authenticated";
        return response;
    }
    
    // Check if token is expired
    if (std::chrono::system_clock::now() >= credentials_.token_expiry) {
        if (!refresh_token()) {
            ApiResponse response;
            response.success = false;
            response.error_message = "Failed to refresh token";
            return response;
        }
    }
    
    try {
        // Create request with authentication
        json auth_params = params;
        auth_params["access_token"] = credentials_.access_token;
        
        return public_request(method, auth_params);
    } catch (const std::exception& e) {
        ApiResponse response;
        response.success = false;
        response.error_message = std::string("Private request failed: ") + e.what();
        return response;
    }
}

bool ApiClient::connect_websocket() {
    std::lock_guard<std::mutex> lock(websocket_mutex_);
    
    if (websocket_connected_) {
        return true;
    }
    
    try {
        // Set message handler
        websocket_client_->set_message_handler(
            std::bind(&ApiClient::websocket_message_handler, this, std::placeholders::_1, std::placeholders::_2)
        );
        
        // Connect to WebSocket server
        websocketpp::lib::error_code ec;
        WebSocketClient::connection_ptr con = websocket_client_->get_connection(websocket_url_, ec);
        
        if (ec) {
            std::cerr << "WebSocket connection error: " << ec.message() << std::endl;
            return false;
        }
        
        websocket_connection_ = con->get_handle();
        websocket_client_->connect(con);
        
        // Start WebSocket thread
        websocket_thread_ = std::thread([this]() {
            try {
                websocket_client_->run();
            } catch (const std::exception& e) {
                std::cerr << "WebSocket thread error: " << e.what() << std::endl;
            }
        });
        
        websocket_connected_ = true;
        
        // Authenticate over WebSocket if we have credentials
        if (authenticated_) {
            json auth_params = {
                {"grant_type", "client_credentials"},
                {"client_id", api_key_},
                {"client_secret", api_secret_}
            };
            
            json auth_request = {
                {"jsonrpc", "2.0"},
                {"id", 42},
                {"method", "public/auth"},
                {"params", auth_params}
            };
            
            websocket_client_->send(websocket_connection_, auth_request.dump(), websocketpp::frame::opcode::text);
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error connecting to WebSocket: " << e.what() << std::endl;
        return false;
    }
}

void ApiClient::disconnect_websocket() {
    std::lock_guard<std::mutex> lock(websocket_mutex_);
    
    if (!websocket_connected_) {
        return;
    }
    
    try {
        // Close WebSocket connection
        websocketpp::lib::error_code ec;
        websocket_client_->close(websocket_connection_, websocketpp::close::status::normal, "Disconnecting", ec);
        
        // Stop WebSocket thread
        if (websocket_thread_.joinable()) {
            websocket_thread_.join();
        }
        
        websocket_connected_ = false;
    } catch (const std::exception& e) {
        std::cerr << "Error disconnecting from WebSocket: " << e.what() << std::endl;
    }
}

bool ApiClient::subscribe(const std::string& channel, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(websocket_mutex_);
    
    if (!websocket_connected_) {
        std::cerr << "Cannot subscribe: WebSocket not connected" << std::endl;
        return false;
    }
    
    try {
        // Store callback
        channel_callbacks_[channel] = callback;
        
        // Create subscription request
        json params = {
            {"channels", {channel}}
        };
        
        json request = {
            {"jsonrpc", "2.0"},
            {"id", 42},
            {"method", "public/subscribe"},
            {"params", params}
        };
        
        // Send subscription request
        websocketpp::lib::error_code ec;
        websocket_client_->send(websocket_connection_, request.dump(), websocketpp::frame::opcode::text, ec);
        
        if (ec) {
            std::cerr << "WebSocket send error: " << ec.message() << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error subscribing to channel: " << e.what() << std::endl;
        return false;
    }
}

bool ApiClient::unsubscribe(const std::string& channel) {
    std::lock_guard<std::mutex> lock(websocket_mutex_);
    
    if (!websocket_connected_) {
        std::cerr << "Cannot unsubscribe: WebSocket not connected" << std::endl;
        return false;
    }
    
    try {
        // Create unsubscription request
        json params = {
            {"channels", {channel}}
        };
        
        json request = {
            {"jsonrpc", "2.0"},
            {"id", 42},
            {"method", "public/unsubscribe"},
            {"params", params}
        };
        
        // Send unsubscription request
        websocketpp::lib::error_code ec;
        websocket_client_->send(websocket_connection_, request.dump(), websocketpp::frame::opcode::text, ec);
        
        if (ec) {
            std::cerr << "WebSocket send error: " << ec.message() << std::endl;
            return false;
        }
        
        // Remove callback
        channel_callbacks_.erase(channel);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error unsubscribing from channel: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> ApiClient::get_instruments(const std::string& currency, InstrumentType type) {
    std::vector<std::string> instruments;
    
    try {
        // Create request parameters
        json params = {
            {"currency", currency},
            {"kind", instrument_type_to_string(type)},
            {"expired", false}
        };
        
        // Make request
        ApiResponse response = public_request("public/get_instruments", params);
        
        if (response.success) {
            // Extract instrument names
            for (const auto& instrument : response.data["result"]) {
                instruments.push_back(instrument["instrument_name"]);
            }
        } else {
            std::cerr << "Error getting instruments: " << response.error_message << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting instruments: " << e.what() << std::endl;
    }
    
    return instruments;
}

bool ApiClient::is_authenticated() const {
    return authenticated_;
}

bool ApiClient::is_websocket_connected() const {
    return websocket_connected_;
}

std::string ApiClient::get_api_url() const {
    return api_url_;
}

std::string ApiClient::get_websocket_url() const {
    return websocket_url_;
}

bool ApiClient::refresh_token() {
    try {
        // Create refresh token request
        json params = {
            {"grant_type", "refresh_token"},
            {"refresh_token", credentials_.refresh_token}
        };
        
        // Make request
        ApiResponse response = public_request("public/auth", params);
        
        if (response.success) {
            // Update tokens
            credentials_.access_token = response.data["result"]["access_token"];
            credentials_.refresh_token = response.data["result"]["refresh_token"];
            
            // Calculate token expiry time
            int expires_in = response.data["result"]["expires_in"];
            credentials_.token_expiry = std::chrono::system_clock::now() + std::chrono::seconds(expires_in);
            
            return true;
        } else {
            std::cerr << "Token refresh failed: " << response.error_message << std::endl;
            authenticated_ = false;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error refreshing token: " << e.what() << std::endl;
        authenticated_ = false;
        return false;
    }
}

void ApiClient::websocket_message_handler(websocketpp::connection_hdl hdl, WebSocketClient::message_ptr msg) {
    try {
        // Parse message
        json message = json::parse(msg->get_payload());
        
        // Handle different message types
        if (message.contains("method") && message["method"] == "subscription") {
            // Subscription message
            std::string channel = message["params"]["channel"];
            
            // Find callback for this channel
            auto it = channel_callbacks_.find(channel);
            if (it != channel_callbacks_.end()) {
                // Add message to queue for processing
                std::lock_guard<std::mutex> lock(queue_mutex_);
                message_queue_.push(std::make_pair(channel, message["params"]["data"]));
                queue_condition_.notify_one();
            }
        } else if (message.contains("id") && message.contains("result")) {
            // Response to a request
            // Currently not handling these specifically
        } else if (message.contains("error")) {
            // Error message
            std::cerr << "WebSocket error: " << message["error"]["message"] << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling WebSocket message: " << e.what() << std::endl;
    }
}

void ApiClient::process_message_queue() {
    while (running_) {
        std::pair<std::string, json> message;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_condition_.wait(lock, [this] { return !message_queue_.empty() || !running_; });
            
            if (!running_) {
                break;
            }
            
            message = message_queue_.front();
            message_queue_.pop();
        }
        
        try {
            // Find callback for this channel
            auto it = channel_callbacks_.find(message.first);
            if (it != channel_callbacks_.end()) {
                // Call callback
                it->second(message.second);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing message: " << e.what() << std::endl;
        }
    }
}

std::string ApiClient::create_signature(const std::string& method, const std::string& path, const std::string& nonce, const std::string& data) {
    // Create string to sign
    std::string string_to_sign = nonce + method + path + data;
    
    // Create HMAC-SHA256 signature
    unsigned char hmac[EVP_MAX_MD_SIZE];
    unsigned int hmac_len;
    
    HMAC(EVP_sha256(), api_secret_.c_str(), api_secret_.length(),
         reinterpret_cast<const unsigned char*>(string_to_sign.c_str()), string_to_sign.length(),
         hmac, &hmac_len);
    
    // Convert to hex string
    std::stringstream ss;
    for (unsigned int i = 0; i < hmac_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hmac[i]);
    }
    
    return ss.str();
}

std::string ApiClient::instrument_type_to_string(InstrumentType type) {
    switch (type) {
        case InstrumentType::SPOT:
            return "spot";
        case InstrumentType::FUTURES:
            return "future";
        case InstrumentType::OPTIONS:
            return "option";
        default:
            return "";
    }
}

} // namespace deribit