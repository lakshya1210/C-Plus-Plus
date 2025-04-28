# Deribit Trading System

A high-performance order execution and management system for trading on Deribit Test (https://test.deribit.com/) using C++.

## Objective

Create a low-latency trading system that can interact with Deribit Test API to execute trades, manage orders, and stream real-time market data.

## Core Requirements

### Order Management Functions
1. Place order
2. Cancel order
3. Modify order
4. Get orderbook
5. View current positions
6. Real-time market data streaming via WebSocket
   - Implement WebSocket server functionality
   - Allow clients to subscribe to symbols
   - Stream continuous orderbook updates for subscribed symbols

### Market Coverage
- Instruments: Spot, Futures, and Options
- Scope: All supported symbols

### Technical Requirements
1. Implementation in C++
2. Low-latency performance
3. Proper error handling and logging
4. WebSocket server for real-time data distribution
   - Connection management
   - Subscription handling
   - Efficient message broadcasting

## Performance Analysis and Optimization

### Latency Benchmarking
1. Measure and document the following metrics:
   - Order placement latency
   - Market data processing latency
   - WebSocket message propagation delay
   - End-to-end trading loop latency

### Optimization Areas
1. Memory management
2. Network communication
3. Data structure selection
4. Thread management
5. CPU optimization

## Setup Instructions

1. Create a new Deribit Test account
2. Generate API Keys for authentication
3. Configure the system with your API credentials
4. Build the project using the provided build instructions

## Project Structure

```
├── include/            # Header files
├── src/                # Source files
├── lib/                # External libraries
├── tests/              # Test files
├── examples/           # Example usage
├── docs/               # Documentation
├── CMakeLists.txt      # CMake build configuration
└── README.md           # This file
```

## Building the Project

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

See the examples directory for sample usage of the trading system.