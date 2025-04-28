#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "deribit_api_client.h"

// Mock API credentials for testing
const std::string TEST_API_KEY = "test_api_key";
const std::string TEST_API_SECRET = "test_api_secret";

class ApiClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create API client with test credentials
        api_client_ = std::make_shared<deribit::ApiClient>(TEST_API_KEY, TEST_API_SECRET, true);
        
        // Initialize API client
        api_client_->initialize();
    }
    
    void TearDown() override {
        // Clean up
        api_client_.reset();
    }
    
    std::shared_ptr<deribit::ApiClient> api_client_;
};

// Test initialization
TEST_F(ApiClientTest, Initialization) {
    EXPECT_TRUE(api_client_ != nullptr);
    EXPECT_FALSE(api_client_->is_authenticated());
    EXPECT_FALSE(api_client_->is_websocket_connected());
    EXPECT_EQ(api_client_->get_api_url(), "https://test.deribit.com");
    EXPECT_EQ(api_client_->get_websocket_url(), "wss://test.deribit.com/ws/api/v2");
}

// Test public request (without authentication)
TEST_F(ApiClientTest, PublicRequest) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API connection";
    
    // Get server time
    deribit::ApiResponse response = api_client_->public_request("public/get_time");
    
    // Check response
    EXPECT_TRUE(response.success);
    EXPECT_TRUE(response.data.contains("result"));
    EXPECT_TRUE(response.data["result"].contains("server_time"));
}

// Test authentication
TEST_F(ApiClientTest, Authentication) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API credentials";
    
    // Authenticate
    bool result = api_client_->authenticate();
    
    // Check result
    EXPECT_TRUE(result);
    EXPECT_TRUE(api_client_->is_authenticated());
}

// Test private request (with authentication)
TEST_F(ApiClientTest, PrivateRequest) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API credentials";
    
    // Authenticate first
    api_client_->authenticate();
    
    // Get account summary
    deribit::ApiResponse response = api_client_->private_request("private/get_account_summary", {{"currency", "BTC"}});
    
    // Check response
    EXPECT_TRUE(response.success);
    EXPECT_TRUE(response.data.contains("result"));
    EXPECT_TRUE(response.data["result"].contains("equity"));
}

// Test WebSocket connection
TEST_F(ApiClientTest, WebSocketConnection) {
    // Skip actual WebSocket connection in unit tests
    GTEST_SKIP() << "Skipping test that requires actual WebSocket connection";
    
    // Connect to WebSocket
    bool result = api_client_->connect_websocket();
    
    // Check result
    EXPECT_TRUE(result);
    EXPECT_TRUE(api_client_->is_websocket_connected());
    
    // Disconnect from WebSocket
    api_client_->disconnect_websocket();
    
    // Check result
    EXPECT_FALSE(api_client_->is_websocket_connected());
}

// Test subscription
TEST_F(ApiClientTest, Subscription) {
    // Skip actual WebSocket connection in unit tests
    GTEST_SKIP() << "Skipping test that requires actual WebSocket connection";
    
    // Connect to WebSocket
    api_client_->connect_websocket();
    
    // Subscribe to channel
    bool result = api_client_->subscribe("book.BTC-PERPETUAL.100ms", [](const deribit::json& data) {
        // Do nothing in test
    });
    
    // Check result
    EXPECT_TRUE(result);
    
    // Unsubscribe from channel
    result = api_client_->unsubscribe("book.BTC-PERPETUAL.100ms");
    
    // Check result
    EXPECT_TRUE(result);
    
    // Disconnect from WebSocket
    api_client_->disconnect_websocket();
}

// Test getting instruments
TEST_F(ApiClientTest, GetInstruments) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API connection";
    
    // Get BTC futures
    std::vector<std::string> instruments = api_client_->get_instruments("BTC", deribit::InstrumentType::FUTURES);
    
    // Check result
    EXPECT_FALSE(instruments.empty());
    
    // Check if BTC-PERPETUAL is in the list
    bool found = false;
    for (const auto& instrument : instruments) {
        if (instrument == "BTC-PERPETUAL") {
            found = true;
            break;
        }
    }
    
    EXPECT_TRUE(found);
}