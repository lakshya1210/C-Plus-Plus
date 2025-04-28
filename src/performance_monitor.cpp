#include "performance_monitor.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace deribit {

// LatencyTracker implementation
LatencyTracker::LatencyTracker(const std::string& name, bool store_samples, size_t max_samples)
    : name_(name) {
    metrics_.name = name;
    metrics_.store_samples = store_samples;
    metrics_.max_samples = max_samples;
}

uint64_t LatencyTracker::start() {
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t id = next_id_++;
    
    std::lock_guard<std::mutex> lock(mutex_);
    start_times_[id] = now;
    
    return id;
}

void LatencyTracker::end(uint64_t id) {
    auto now = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = start_times_.find(id);
    if (it == start_times_.end()) {
        std::cerr << "Warning: No start time found for ID " << id << std::endl;
        return;
    }
    
    auto start_time = it->second;
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_time);
    
    // Update metrics
    metrics_.min_latency = std::min(metrics_.min_latency, latency);
    metrics_.max_latency = std::max(metrics_.max_latency, latency);
    metrics_.total_latency += latency;
    metrics_.count++;
    
    // Store sample if enabled
    if (metrics_.store_samples && metrics_.samples.size() < metrics_.max_samples) {
        metrics_.samples.push_back(latency);
    }
    
    // Remove start time
    start_times_.erase(it);
}

LatencyMetric LatencyTracker::get_metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_;
}

void LatencyTracker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    metrics_.min_latency = std::chrono::nanoseconds::max();
    metrics_.max_latency = std::chrono::nanoseconds::min();
    metrics_.total_latency = std::chrono::nanoseconds(0);
    metrics_.count = 0;
    metrics_.samples.clear();
}

// LatencyMetric implementation
double LatencyMetric::percentile_latency_ns(double percentile) const {
    if (samples.empty()) {
        return 0.0;
    }
    
    // Make a copy of the samples
    std::vector<std::chrono::nanoseconds> sorted_samples = samples;
    
    // Sort the samples
    std::sort(sorted_samples.begin(), sorted_samples.end());
    
    // Calculate the index
    double index = percentile * (sorted_samples.size() - 1) / 100.0;
    size_t lower_index = static_cast<size_t>(std::floor(index));
    size_t upper_index = static_cast<size_t>(std::ceil(index));
    
    // Handle edge cases
    if (lower_index == upper_index) {
        return sorted_samples[lower_index].count();
    }
    
    // Interpolate
    double weight = index - lower_index;
    return sorted_samples[lower_index].count() * (1.0 - weight) + 
           sorted_samples[upper_index].count() * weight;
}

// PerformanceMonitor implementation
PerformanceMonitor& PerformanceMonitor::instance() {
    static PerformanceMonitor instance;
    return instance;
}

std::shared_ptr<LatencyTracker> PerformanceMonitor::get_tracker(const std::string& name, 
                                                              bool store_samples, 
                                                              size_t max_samples) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = trackers_.find(name);
    if (it != trackers_.end()) {
        return it->second;
    }
    
    auto tracker = std::make_shared<LatencyTracker>(name, store_samples, max_samples);
    trackers_[name] = tracker;
    
    return tracker;
}

std::map<std::string, LatencyMetric> PerformanceMonitor::get_all_metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<std::string, LatencyMetric> metrics;
    
    for (const auto& pair : trackers_) {
        metrics[pair.first] = pair.second->get_metrics();
    }
    
    return metrics;
}

std::shared_ptr<LatencyMetric> PerformanceMonitor::get_metrics(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = trackers_.find(name);
    if (it != trackers_.end()) {
        auto metrics = std::make_shared<LatencyMetric>(it->second->get_metrics());
        return metrics;
    }
    
    return nullptr;
}

void PerformanceMonitor::reset_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : trackers_) {
        pair.second->reset();
    }
}

void PerformanceMonitor::reset(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = trackers_.find(name);
    if (it != trackers_.end()) {
        it->second->reset();
    }
}

bool PerformanceMonitor::export_to_csv(const std::string& filename) const {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error opening file: " << filename << std::endl;
            return false;
        }
        
        // Write header
        file << "Operation,Count,Min (ns),Max (ns),Avg (ns),Avg (us),Avg (ms),P50 (ns),P90 (ns),P99 (ns)\n";
        
        // Get all metrics
        auto metrics = get_all_metrics();
        
        // Write data
        for (const auto& pair : metrics) {
            const auto& metric = pair.second;
            
            file << metric.name << ","
                 << metric.count << ","
                 << metric.min_latency.count() << ","
                 << metric.max_latency.count() << ","
                 << metric.average_latency_ns() << ","
                 << metric.average_latency_us() << ","
                 << metric.average_latency_ms() << ",";
            
            // Add percentiles if samples are available
            if (!metric.samples.empty()) {
                file << metric.percentile_latency_ns(50) << ","
                     << metric.percentile_latency_ns(90) << ","
                     << metric.percentile_latency_ns(99);
            } else {
                file << "N/A,N/A,N/A";
            }
            
            file << "\n";
        }
        
        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error exporting metrics to CSV: " << e.what() << std::endl;
        return false;
    }
}

void PerformanceMonitor::print_metrics() const {
    // Get all metrics
    auto metrics = get_all_metrics();
    
    // Print header
    std::cout << std::left << std::setw(30) << "Operation"
              << std::right << std::setw(10) << "Count"
              << std::right << std::setw(15) << "Min (us)"
              << std::right << std::setw(15) << "Max (us)"
              << std::right << std::setw(15) << "Avg (us)"
              << std::right << std::setw(15) << "P50 (us)"
              << std::right << std::setw(15) << "P90 (us)"
              << std::right << std::setw(15) << "P99 (us)"
              << std::endl;
    
    std::cout << std::string(130, '-') << std::endl;
    
    // Print data
    for (const auto& pair : metrics) {
        const auto& metric = pair.second;
        
        std::cout << std::left << std::setw(30) << metric.name
                  << std::right << std::setw(10) << metric.count
                  << std::right << std::setw(15) << std::fixed << std::setprecision(3) << (metric.min_latency.count() / 1000.0)
                  << std::right << std::setw(15) << std::fixed << std::setprecision(3) << (metric.max_latency.count() / 1000.0)
                  << std::right << std::setw(15) << std::fixed << std::setprecision(3) << metric.average_latency_us();
        
        // Add percentiles if samples are available
        if (!metric.samples.empty()) {
            std::cout << std::right << std::setw(15) << std::fixed << std::setprecision(3) << (metric.percentile_latency_ns(50) / 1000.0)
                      << std::right << std::setw(15) << std::fixed << std::setprecision(3) << (metric.percentile_latency_ns(90) / 1000.0)
                      << std::right << std::setw(15) << std::fixed << std::setprecision(3) << (metric.percentile_latency_ns(99) / 1000.0);
        } else {
            std::cout << std::right << std::setw(15) << "N/A"
                      << std::right << std::setw(15) << "N/A"
                      << std::right << std::setw(15) << "N/A";
        }
        
        std::cout << std::endl;
    }
}

// ScopedLatencyTracker implementation
ScopedLatencyTracker::ScopedLatencyTracker(std::shared_ptr<LatencyTracker> tracker)
    : tracker_(tracker) {
    id_ = tracker_->start();
}

ScopedLatencyTracker::~ScopedLatencyTracker() {
    tracker_->end(id_);
}

} // namespace deribit