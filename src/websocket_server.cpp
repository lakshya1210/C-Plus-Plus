#include "websocket_server.h"
#include <iostream>
#include <sstream>
#include "performance_monitor.h"

namespace deribit {

WebSocketServer::WebSocketServer(std::shared_ptr<ApiClient> api_client,
                               std::shared_ptr<OrderManager> order_manager,
                               uint16_t port)
    : api_client_(api_client),
      order_manager_(order_manager),
      port_(port),
      running_(false) {
    // Validate parameters
    if (!api_client_) {
        throw std::invalid_argument("API client cannot be null");
    }
    
    if (!order_manager_) {
        throw std::invalid_argument("Order manager cannot be null");
    }
}

WebSocketServer::~WebSocketServer() {
    // Stop server if running
    if (running_) {
        stop();
    }
}

bool WebSocketServer::initialize() {
    try {
        // Create server
        server_ = std::make_unique<WebSocketServerType>();
        
        // Set logging settings
        server_->set_access_channels(websocketpp::log::alevel::none);
        server_->set_error_channels(websocketpp::log::elevel::fatal);
        
        // Initialize ASIO
        server_->init_asio();
        
        // Set handlers
        server_->set_open_handler(std::bind(&WebSocketServer::on_open, this, std::placeholders::_1));
        server_->set_close_handler(std::bind(&WebSocketServer::on_close, this, std::placeholders::_1));
        server_->set_message_handler(std::bind(&WebSocketServer::on_message, this, std::placeholders::_1, std::placeholders::_2));
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing WebSocket server: " << e.what() << std::endl;
        return false;
    }
}

bool WebSocketServer::start() {
    if (running_) {
        return true;
    }
    
    try {
        // Listen on port
        server_->listen(port_);
        
        // Start accept loop
        server_->start_accept();
        
        // Start server thread
        server_thread_ = std::thread([this]() {
            try {
                server_->run();
            } catch (const std::exception& e) {
                std::cerr << "WebSocket server thread error: " << e.what() << std::endl;
            }
        });
        
        running_ = true;
        
        std::cout << "WebSocket server started on port " << port_ << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error starting WebSocket server: " << e.what() << std::endl;
        return false;
    }
}

void WebSocketServer::stop() {
    if (!running_) {
        return;
    }
    
    try {
        // Stop server
        server_->stop();
        
        // Join server thread
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        
        running_ = false;
        
        std::cout << "WebSocket server stopped" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error stopping WebSocket server: " << e.what() << std::endl;
    }
}

bool WebSocketServer::is_running() const {
    return running_;
}

void WebSocketServer::set_open_callback(ConnectionCallback callback) {
    open_callback_ = callback;
}

void WebSocketServer::set_close_callback(ConnectionCallback callback) {
    close_callback_ = callback;
}

void WebSocketServer::set_message_callback(MessageCallback callback) {
    message_callback_ = callback;
}

void WebSocketServer::broadcast(const std::string& message) {
    // Start latency tracking
    auto tracker = PerformanceMonitor::instance().get_tracker("broadcast_message", true);
    auto tracking_id = tracker->start();
    
    try {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        for (auto& hdl : connections_) {
            try {
                server_->send(hdl, message, websocketpp::frame::opcode::text);
            } catch (const std::exception& e) {
                std::cerr << "Error sending message to client: " << e.what() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error broadcasting message: " << e.what() << std::endl;
    }
    
    // End latency tracking
    tracker->end(tracking_id);
}

void WebSocketServer::broadcast_to_channel(const std::string& channel, const std::string& message) {
    // Start latency tracking
    auto tracker = PerformanceMonitor::instance().get_tracker("broadcast_to_channel", true);
    auto tracking_id = tracker->start();
    
    try {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        auto it = channel_subscriptions_.find(channel);
        if (it != channel_subscriptions_.end()) {
            for (auto& hdl : it->second) {
                try {
                    server_->send(hdl, message, websocketpp::frame::opcode::text);
                } catch (const std::exception& e) {
                    std::cerr << "Error sending message to client: " << e.what() << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error broadcasting message to channel: " << e.what() << std::endl;
    }
    
    // End latency tracking
    tracker->end(tracking_id);
}

void WebSocketServer::send(ConnectionHandle hdl, const std::string& message) {
    try {
        server_->send(hdl, message, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        std::cerr << "Error sending message to client: " << e.what() << std::endl;
    }
}

size_t WebSocketServer::get_connection_count() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return connections_.size();
}

void WebSocketServer::handle_orderbook_update(const std::string& instrument_name, const OrderBook& orderbook) {
    // Start latency tracking
    auto tracker = PerformanceMonitor::instance().get_tracker("handle_orderbook_update", true);
    auto tracking_id = tracker->start();
    
    try {
        // Create message
        json message = {
            {"type", "orderbook"},
            {"instrument_name", instrument_name},
            {"timestamp", orderbook.timestamp},
            {"bids", json::array()},
            {"asks", json::array()}
        };
        
        // Add bids
        for (const auto& bid : orderbook.bids) {
            message["bids"].push_back({bid.first, bid.second});
        }
        
        // Add asks
        for (const auto& ask : orderbook.asks) {
            message["asks"].push_back({ask.first, ask.second});
        }
        
        // Broadcast to subscribers
        std::string channel = "orderbook." + instrument_name;
        broadcast_to_channel(channel, message.dump());
    } catch (const std::exception& e) {
        std::cerr << "Error handling orderbook update: " << e.what() << std::endl;
    }
    
    // End latency tracking
    tracker->end(tracking_id);
}

void WebSocketServer::on_open(ConnectionHandle hdl) {
    try {
        // Add to connections
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.insert(hdl);
        }
        
        // Send welcome message
        json welcome = {
            {"type", "welcome"},
            {"message", "Welcome to Deribit Trading System WebSocket Server"}
        };
        
        send(hdl, welcome.dump());
        
        // Call user callback if set
        if (open_callback_) {
            open_callback_(hdl);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling connection open: " << e.what() << std::endl;
    }
}

void WebSocketServer::on_close(ConnectionHandle hdl) {
    try {
        // Remove from connections
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.erase(hdl);
        }
        
        // Remove from subscriptions
        unsubscribe_all(hdl);
        
        // Call user callback if set
        if (close_callback_) {
            close_callback_(hdl);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling connection close: " << e.what() << std::endl;
    }
}

void WebSocketServer::on_message(ConnectionHandle hdl, MessagePtr msg) {
    try {
        // Process message
        process_message(hdl, msg->get_payload());
        
        // Call user callback if set
        if (message_callback_) {
            message_callback_(hdl, msg);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling message: " << e.what() << std::endl;
        
        // Send error response
        json error = {
            {"type", "error"},
            {"message", std::string("Error processing message: ") + e.what()}
        };
        
        send(hdl, error.dump());
    }
}

bool WebSocketServer::subscribe_client(ConnectionHandle hdl, const std::string& channel) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    // Add to channel subscriptions
    channel_subscriptions_[channel].insert(hdl);
    
    // Add to connection subscriptions
    connection_subscriptions_[hdl].insert(channel);
    
    return true;
}

bool WebSocketServer::unsubscribe_client(ConnectionHandle hdl, const std::string& channel) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    // Remove from channel subscriptions
    auto channel_it = channel_subscriptions_.find(channel);
    if (channel_it != channel_subscriptions_.end()) {
        channel_it->second.erase(hdl);
        
        // Remove channel if empty
        if (channel_it->second.empty()) {
            channel_subscriptions_.erase(channel_it);
        }
    }
    
    // Remove from connection subscriptions
    auto connection_it = connection_subscriptions_.find(hdl);
    if (connection_it != connection_subscriptions_.end()) {
        connection_it->second.erase(channel);
        
        // Remove connection if empty
        if (connection_it->second.empty()) {
            connection_subscriptions_.erase(connection_it);
        }
    }
    
    return true;
}

void WebSocketServer::unsubscribe_all(ConnectionHandle hdl) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    // Get channels for this connection
    auto connection_it = connection_subscriptions_.find(hdl);
    if (connection_it != connection_subscriptions_.end()) {
        // Copy channels to avoid iterator invalidation
        std::set<std::string> channels = connection_it->second;
        
        // Remove from each channel
        for (const auto& channel : channels) {
            auto channel_it = channel_subscriptions_.find(channel);
            if (channel_it != channel_subscriptions_.end()) {
                channel_it->second.erase(hdl);
                
                // Remove channel if empty
                if (channel_it->second.empty()) {
                    channel_subscriptions_.erase(channel_it);
                }
            }
        }
        
        // Remove connection subscriptions
        connection_subscriptions_.erase(connection_it);
    }
}

void WebSocketServer::process_message(ConnectionHandle hdl, const std::string& message) {
    try {
        // Parse message
        json request = json::parse(message);
        
        // Check message type
        if (!request.contains("type")) {
            throw std::invalid_argument("Message missing 'type' field");
        }
        
        std::string type = request["type"];
        
        if (type == "subscribe") {
            handle_subscribe_request(hdl, request);
        } else if (type == "unsubscribe") {
            handle_unsubscribe_request(hdl, request);
        } else {
            throw std::invalid_argument("Unknown message type: " + type);
        }
    } catch (const json::exception& e) {
        throw std::invalid_argument(std::string("Invalid JSON: ") + e.what());
    }
}

void WebSocketServer::handle_subscribe_request(ConnectionHandle hdl, const json& request) {
    // Check required fields
    if (!request.contains("channel")) {
        throw std::invalid_argument("Subscribe message missing 'channel' field");
    }
    
    std::string channel = request["channel"];
    
    // Subscribe client
    if (subscribe_client(hdl, channel)) {
        // Send success response
        json response = {
            {"type", "subscribed"},
            {"channel", channel}
        };
        
        send(hdl, response.dump());
        
        // If this is an orderbook channel, send initial orderbook
        if (channel.substr(0, 10) == "orderbook.") {
            std::string instrument_name = channel.substr(10);
            OrderBook orderbook = order_manager_->get_orderbook(instrument_name);
            handle_orderbook_update(instrument_name, orderbook);
        }
    } else {
        // Send error response
        json error = {
            {"type", "error"},
            {"message", "Failed to subscribe to channel: " + channel}
        };
        
        send(hdl, error.dump());
    }
}

void WebSocketServer::handle_unsubscribe_request(ConnectionHandle hdl, const json& request) {
    // Check required fields
    if (!request.contains("channel")) {
        throw std::invalid_argument("Unsubscribe message missing 'channel' field");
    }
    
    std::string channel = request["channel"];
    
    // Unsubscribe client
    if (unsubscribe_client(hdl, channel)) {
        // Send success response
        json response = {
            {"type", "unsubscribed"},
            {"channel", channel}
        };
        
        send(hdl, response.dump());
    } else {
        // Send error response
        json error = {
            {"type", "error"},
            {"message", "Failed to unsubscribe from channel: " + channel}
        };
        
        send(hdl, error.dump());
    }
}

} // namespace deribit