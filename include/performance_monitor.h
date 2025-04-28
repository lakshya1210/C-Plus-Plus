#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <string>
#include <chrono>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <fstream>

namespace deribit {

/**
 * @struct LatencyMetric
 * @brief Stores latency measurements for a specific operation
 */
struct LatencyMetric {
    std::string name;
    std::chrono::nanoseconds min_latency{std::chrono::nanoseconds::max()};
    std::chrono::nanoseconds max_latency{std::chrono::nanoseconds::min()};
    std::chrono::nanoseconds total_latency{0};
    uint64_t count{0};
    std::vector<std::chrono::nanoseconds> samples;
    bool store_samples{false};
    size_t max_samples{1000};

    // Calculate average latency
    double average_latency_ns() const {
        if (count == 0) return 0.0;
        return static_cast<double>(total_latency.count()) / count;
    }

    // Calculate average latency in microseconds
    double average_latency_us() const {
        return average_latency_ns() / 1000.0;
    }

    // Calculate average latency in milliseconds
    double average_latency_ms() const {
        return average_latency_us() / 1000.0;
    }

    // Get percentile latency
    double percentile_latency_ns(double percentile) const;
};

/**
 * @class LatencyTracker
 * @brief Tracks latency for a single operation
 */
class LatencyTracker {
public:
    /**
     * @brief Constructor
     * @param name The name of the operation being tracked
     * @param store_samples Whether to store individual samples (default: false)
     * @param max_samples Maximum number of samples to store (default: 1000)
     */
    explicit LatencyTracker(const std::string& name, bool store_samples = false, size_t max_samples = 1000);

    /**
     * @brief Start timing an operation
     * @return A unique ID for this timing operation
     */
    uint64_t start();

    /**
     * @brief End timing an operation
     * @param id The ID returned by start()
     */
    void end(uint64_t id);

    /**
     * @brief Get the current metrics
     * @return The latency metrics
     */
    LatencyMetric get_metrics() const;

    /**
     * @brief Reset the metrics
     */
    void reset();

private:
    std::string name_;
    std::map<uint64_t, std::chrono::high_resolution_clock::time_point> start_times_;
    LatencyMetric metrics_;
    std::mutex mutex_;
    std::atomic<uint64_t> next_id_{0};
};

/**
 * @class PerformanceMonitor
 * @brief Monitors performance metrics across the system
 */
class PerformanceMonitor {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static PerformanceMonitor& instance();

    /**
     * @brief Get a latency tracker for an operation
     * @param name The name of the operation
     * @param store_samples Whether to store individual samples (default: false)
     * @param max_samples Maximum number of samples to store (default: 1000)
     * @return Shared pointer to the latency tracker
     */
    std::shared_ptr<LatencyTracker> get_tracker(const std::string& name, 
                                              bool store_samples = false, 
                                              size_t max_samples = 1000);

    /**
     * @brief Get all metrics
     * @return Map of operation names to latency metrics
     */
    std::map<std::string, LatencyMetric> get_all_metrics() const;

    /**
     * @brief Get metrics for a specific operation
     * @param name The name of the operation
     * @return The latency metrics, or nullptr if not found
     */
    std::shared_ptr<LatencyMetric> get_metrics(const std::string& name) const;

    /**
     * @brief Reset all metrics
     */
    void reset_all();

    /**
     * @brief Reset metrics for a specific operation
     * @param name The name of the operation
     */
    void reset(const std::string& name);

    /**
     * @brief Export metrics to a CSV file
     * @param filename The name of the file to export to
     * @return true if export successful, false otherwise
     */
    bool export_to_csv(const std::string& filename) const;

    /**
     * @brief Print metrics to the console
     */
    void print_metrics() const;

private:
    // Private constructor for singleton
    PerformanceMonitor() = default;
    
    // Delete copy constructor and assignment operator
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;

    std::map<std::string, std::shared_ptr<LatencyTracker>> trackers_;
    mutable std::mutex mutex_;
};

/**
 * @class ScopedLatencyTracker
 * @brief RAII class for tracking latency of a scope
 */
class ScopedLatencyTracker {
public:
    /**
     * @brief Constructor
     * @param tracker The latency tracker to use
     */
    explicit ScopedLatencyTracker(std::shared_ptr<LatencyTracker> tracker);

    /**
     * @brief Destructor
     */
    ~ScopedLatencyTracker();

private:
    std::shared_ptr<LatencyTracker> tracker_;
    uint64_t id_;
};

// Convenience macro for scoped latency tracking
#define TRACK_LATENCY(name) \
    auto tracker = deribit::PerformanceMonitor::instance().get_tracker(name); \
    deribit::ScopedLatencyTracker scoped_tracker(tracker);

} // namespace deribit

#endif // PERFORMANCE_MONITOR_H