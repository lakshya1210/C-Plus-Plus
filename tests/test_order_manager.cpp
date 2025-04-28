#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "deribit_api_client.h"
#include "order_manager.h"

// Mock API credentials for testing
const std::string TEST_API_KEY = "test_api_key";
const std::string TEST_API_SECRET = "test_api_secret";

class OrderManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create API client with test credentials
        api_client_ = std::make_shared<deribit::ApiClient>(TEST_API_KEY, TEST_API_SECRET, true);
        
        // Initialize API client
        api_client_->initialize();
        
        // Create order manager
        order_manager_ = std::make_shared<deribit::OrderManager>(api_client_);
    }
    
    void TearDown() override {
        // Clean up
        order_manager_.reset();
        api_client_.reset();
    }
    
    std::shared_ptr<deribit::ApiClient> api_client_;
    std::shared_ptr<deribit::OrderManager> order_manager_;
};

// Test order manager creation
TEST_F(OrderManagerTest, Creation) {
    EXPECT_TRUE(order_manager_ != nullptr);
}

// Test placing an order
TEST_F(OrderManagerTest, PlaceOrder) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API credentials";
    
    // Authenticate first
    api_client_->authenticate();
    
    // Place a limit order
    std::string order_id = order_manager_->place_order(
        "BTC-PERPETUAL",
        deribit::OrderType::LIMIT,
        deribit::OrderDirection::BUY,
        0.1,  // amount
        10000.0,  // price
        deribit::TimeInForce::GOOD_TIL_CANCELLED
    );
    
    // Check result
    EXPECT_FALSE(order_id.empty());
    
    // Clean up
    order_manager_->cancel_order(order_id);
}

// Test canceling an order
TEST_F(OrderManagerTest, CancelOrder) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API credentials";
    
    // Authenticate first
    api_client_->authenticate();
    
    // Place a limit order
    std::string order_id = order_manager_->place_order(
        "BTC-PERPETUAL",
        deribit::OrderType::LIMIT,
        deribit::OrderDirection::BUY,
        0.1,  // amount
        10000.0,  // price
        deribit::TimeInForce::GOOD_TIL_CANCELLED
    );
    
    // Cancel the order
    bool result = order_manager_->cancel_order(order_id);
    
    // Check result
    EXPECT_TRUE(result);
}

// Test modifying an order
TEST_F(OrderManagerTest, ModifyOrder) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API credentials";
    
    // Authenticate first
    api_client_->authenticate();
    
    // Place a limit order
    std::string order_id = order_manager_->place_order(
        "BTC-PERPETUAL",
        deribit::OrderType::LIMIT,
        deribit::OrderDirection::BUY,
        0.1,  // amount
        10000.0,  // price
        deribit::TimeInForce::GOOD_TIL_CANCELLED
    );
    
    // Modify the order
    bool result = order_manager_->modify_order(order_id, 0.2, 10500.0);
    
    // Check result
    EXPECT_TRUE(result);
    
    // Clean up
    order_manager_->cancel_order(order_id);
}

// Test getting orderbook
TEST_F(OrderManagerTest, GetOrderbook) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API connection";
    
    // Get orderbook
    deribit::OrderBook orderbook = order_manager_->get_orderbook("BTC-PERPETUAL");
    
    // Check result
    EXPECT_EQ(orderbook.instrument_name, "BTC-PERPETUAL");
    EXPECT_FALSE(orderbook.bids.empty());
    EXPECT_FALSE(orderbook.asks.empty());
    EXPECT_FALSE(orderbook.timestamp.empty());
}

// Test getting positions
TEST_F(OrderManagerTest, GetPositions) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API credentials";
    
    // Authenticate first
    api_client_->authenticate();
    
    // Get positions
    std::vector<deribit::Position> positions = order_manager_->get_positions();
    
    // Check result (may be empty if no positions)
    // Just check that the call doesn't throw
    SUCCEED();
}

// Test getting a specific position
TEST_F(OrderManagerTest, GetPosition) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API credentials";
    
    // Authenticate first
    api_client_->authenticate();
    
    // Get position
    std::shared_ptr<deribit::Position> position = order_manager_->get_position("BTC-PERPETUAL");
    
    // Check result (may be null if no position)
    // Just check that the call doesn't throw
    SUCCEED();
}

// Test getting open orders
TEST_F(OrderManagerTest, GetOpenOrders) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API credentials";
    
    // Authenticate first
    api_client_->authenticate();
    
    // Get open orders
    std::vector<deribit::Order> orders = order_manager_->get_open_orders();
    
    // Check result (may be empty if no open orders)
    // Just check that the call doesn't throw
    SUCCEED();
}

// Test getting a specific order
TEST_F(OrderManagerTest, GetOrder) {
    // Skip actual API call in unit tests
    GTEST_SKIP() << "Skipping test that requires actual API credentials";
    
    // Authenticate first
    api_client_->authenticate();
    
    // Place a limit order
    std::string order_id = order_manager_->place_order(
        "BTC-PERPETUAL",
        deribit::OrderType::LIMIT,
        deribit::OrderDirection::BUY,
        0.1,  // amount
        10000.0,  // price
        deribit::TimeInForce::GOOD_TIL_CANCELLED
    );
    
    // Get order
    std::shared_ptr<deribit::Order> order = order_manager_->get_order(order_id);
    
    // Check result
    EXPECT_TRUE(order != nullptr);
    EXPECT_EQ(order->order_id, order_id);
    EXPECT_EQ(order->instrument_name, "BTC-PERPETUAL");
    EXPECT_EQ(order->type, deribit::OrderType::LIMIT);
    EXPECT_EQ(order->direction, deribit::OrderDirection::BUY);
    EXPECT_DOUBLE_EQ(order->amount, 0.1);
    EXPECT_DOUBLE_EQ(order->price, 10000.0);
    EXPECT_EQ(order->time_in_force, deribit::TimeInForce::GOOD_TIL_CANCELLED);
    
    // Clean up
    order_manager_->cancel_order(order_id);
}