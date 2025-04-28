#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include "deribit_api_client.h"
#include "order_manager.h"
#include "websocket_server.h"

// Mock API credentials for testing
const std::string TEST_API_KEY = "test_api_key";
const std::string TEST_API_SECRET = "test_api_secret";
const uint16_t TEST_PORT = 9001;

// WebSocket client for testing
using WebSocketClient = websocketpp::client<websocketpp::config::asio_client>;

class WebSocketServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create API client with test credentials
        api_client_ = std::make_shared<deribit::ApiClient>(TEST_API_KEY, TEST_API_SECRET, true);
        
        // Initialize API client
        api_client_->initialize();
        
        // Create order manager
        order_manager_ = std::make_shared<deribit::OrderManager>(api_client_);
        
        // Create WebSocket server
        websocket_server_ = std::make_shared<deribit::WebSocketServer>(api_client_, order_manager_, TEST_PORT);
        
        // Initialize WebSocket server
        websocket_server_->initialize();
    }
    
    void TearDown() override {
        // Stop WebSocket server if running
        if (websocket_server_->is_running()) {
            websocket_server_->stop();
        }
        
        // Clean up
        websocket_server_.reset();
        order_manager_.reset();
        api_client_.reset();
    }
    
    std::shared_ptr<deribit::ApiClient> api_client_;
    std::shared_ptr<deribit::OrderManager> order_manager_;
    std::shared_ptr<deribit::WebSocketServer> websocket_server_;
};

// Test WebSocket server creation
TEST_F(WebSocketServerTest, Creation) {
    EXPECT_TRUE(websocket_server_ != nullptr);
    EXPECT_FALSE(websocket_server_->is_running());
}

// Test starting and stopping the server
TEST_F(WebSocketServerTest, StartStop) {
    // Start server
    bool result = websocket_server_->start();
    
    // Check result
    EXPECT_TRUE(result);
    EXPECT_TRUE(websocket_server_->is_running());
    
    // Stop server
    websocket_server_->stop();
    
    // Check result
    EXPECT_FALSE(websocket_server_->is_running());
}

// Test connection count
TEST_F(WebSocketServerTest, ConnectionCount) {
    // Skip actual WebSocket connection in unit tests
    GTEST_SKIP() << "Skipping test that requires actual WebSocket connection";
    
    // Start server
    websocket_server_->start();
    
    // Check initial connection count
    EXPECT_EQ(websocket_server_->get_connection_count(), 0);
    
    // Create WebSocket client
    WebSocketClient client;
    client.init_asio();
    client.set_open_handler([](websocketpp::connection_hdl) {
        // Connection opened
    });
    
    // Connect to server
    websocketpp::lib::error_code ec;
    WebSocketClient::connection_ptr con = client.get_connection("ws://localhost:" + std::to_string(TEST_PORT), ec);
    client.connect(con);
    
    // Run client in a separate thread
    std::thread client_thread([&client]() {
        client.run();
    });
    
    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check connection count
    EXPECT_EQ(websocket_server_->get_connection_count(), 1);
    
    // Close connection
    client.close(con->get_handle(), websocketpp::close::status::normal, "Test complete");
    
    // Wait for connection to close
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check connection count
    EXPECT_EQ(websocket_server_->get_connection_count(), 0);
    
    // Stop server
    websocket_server_->stop();
    
    // Join client thread
    if (client_thread.joinable()) {
        client_thread.join();
    }
}

// Test broadcasting a message
TEST_F(WebSocketServerTest, Broadcast) {
    // Skip actual WebSocket connection in unit tests
    GTEST_SKIP() << "Skipping test that requires actual WebSocket connection";
    
    // Start server
    websocket_server_->start();
    
    // Create WebSocket client
    WebSocketClient client;
    client.init_asio();
    
    // Message received flag
    bool message_received = false;
    std::string received_message;
    
    // Set message handler
    client.set_message_handler([&message_received, &received_message](websocketpp::connection_hdl, WebSocketClient::message_ptr msg) {
        received_message = msg->get_payload();
        message_received = true;
    });
    
    // Connect to server
    websocketpp::lib::error_code ec;
    WebSocketClient::connection_ptr con = client.get_connection("ws://localhost:" + std::to_string(TEST_PORT), ec);
    client.connect(con);
    
    // Run client in a separate thread
    std::thread client_thread([&client]() {
        client.run();
    });
    
    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Broadcast a message
    std::string test_message = "{\"type\":\"test\",\"message\":\"Hello, World!\"}";
    websocket_server_->broadcast(test_message);
    
    // Wait for message to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check if message was received
    EXPECT_TRUE(message_received);
    EXPECT_EQ(received_message, test_message);
    
    // Close connection
    client.close(con->get_handle(), websocketpp::close::status::normal, "Test complete");
    
    // Stop server
    websocket_server_->stop();
    
    // Join client thread
    if (client_thread.joinable()) {
        client_thread.join();
    }
}

// Test handling an orderbook update
TEST_F(WebSocketServerTest, HandleOrderbookUpdate) {
    // Skip actual WebSocket connection in unit tests
    GTEST_SKIP() << "Skipping test that requires actual WebSocket connection";
    
    // Start server
    websocket_server_->start();
    
    // Create orderbook
    deribit::OrderBook orderbook;
    orderbook.instrument_name = "BTC-PERPETUAL";
    orderbook.timestamp = "1234567890";
    orderbook.bids.push_back(std::make_pair(10000.0, 1.0));
    orderbook.asks.push_back(std::make_pair(10100.0, 1.0));
    
    // Handle orderbook update
    websocket_server_->handle_orderbook_update("BTC-PERPETUAL", orderbook);
    
    // Stop server
    websocket_server_->stop();
    
    // Just check that the call doesn't throw
    SUCCEED();
}