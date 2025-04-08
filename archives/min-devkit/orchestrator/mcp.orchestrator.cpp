/// @file
/// @ingroup     mcpmodules
/// @copyright   Copyright 2025 The Max 9-Claude Desktop Integration Project. All rights reserved.
/// @license     Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <functional>
#include <string>
#include <chrono>

using namespace c74::min;

/**
 * @brief Integration Orchestrator for Max 8-Claude Desktop
 *
 * This module implements the integration layer of the 4-layer architecture:
 * - Provides orchestration between components
 * - Routes messages and commands
 * - Manages resource allocation
 * - Facilitates technology selection and mapping
 *
 * It serves as the central coordination point in the architecture:
 * 1. Intelligence Layer ↔ Integration Layer ↔ Execution Layer ↔ Interaction Layer
 */
class mcp_orchestrator : public object<mcp_orchestrator> {
public:
    MIN_DESCRIPTION {"Integration Orchestrator for Max 8-Claude Desktop"};
    MIN_TAGS {"mcp-bridge"};
    MIN_AUTHOR {"Max 8-Claude Integration Team"};
    MIN_RELATED {"mcp.core, mcp.state, mcp.ui, mcp.context"};

    // Inlets and outlets
    inlet<> main_inlet { this, "(bang) initialize orchestrator, (dictionary) process command" };
    outlet<> command_outlet { this, "(dictionary) command routing" };
    outlet<> status_outlet { this, "(dictionary) status information" };
    outlet<> error_outlet { this, "(dictionary) error information" };

    // Core configuration attributes
    attribute<bool> debug_mode { this, "debug", false,
        description {"Enable debug mode for detailed logging"}
    };

    attribute<symbol> routing_strategy { this, "strategy", "priority",
        description {"Message routing strategy: 'priority', 'round-robin', 'load-balanced'"}
    };

    attribute<int> queue_size { this, "queuesize", 64,
        description {"Maximum size of the command queue"},
        range {8, 1024}
    };

    attribute<int> worker_threads { this, "workers", 2,
        description {"Number of worker threads for processing commands"},
        range {1, 8}
    };

    attribute<bool> auto_reconnect { this, "auto_reconnect", true,
        description {"Automatically reconnect to services on failure"}
    };

    // Channel system for component communication
    enum class ChannelType {
        INTELLIGENCE,    // Intelligence layer channel
        EXECUTION,       // Execution layer channel
        INTERACTION,     // Interaction layer channel
        SYSTEM           // System/internal channel
    };

    struct Message {
        ChannelType source;
        ChannelType destination;
        std::string command;
        atoms args;
        int priority;
        std::chrono::system_clock::time_point timestamp;

        Message(ChannelType src, ChannelType dst, std::string cmd, atoms a, int p = 0)
            : source(src), destination(dst), command(std::move(cmd)), args(std::move(a)), priority(p),
              timestamp(std::chrono::system_clock::now()) {}
    };

    // Technology mapping for execution layer
    enum class TechnologyType {
        JAVASCRIPT_V8UI,     // JavaScript in v8ui environment
        NODE_SCRIPT,         // Node.js script
        CPP_MIN_DEVKIT,      // C++ Min-DevKit native
        OSC_PROTOCOL         // OSC communication protocol
    };

    // Component registry
    class ComponentRegistry {
    public:
        void register_component(const std::string& name, ChannelType type, const std::string& capabilities) {
            std::lock_guard<std::mutex> lock(mutex_);
            components_[name] = {type, capabilities};
        }

        bool has_component(const std::string& name) const {
            std::lock_guard<std::mutex> lock(mutex_);
            return components_.find(name) != components_.end();
        }

        ChannelType get_component_type(const std::string& name) const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = components_.find(name);
            if (it != components_.end()) {
                return it->second.first;
            }
            throw std::runtime_error("Component not found: " + name);
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::string, std::pair<ChannelType, std::string>> components_;
    };

    // Technology selector for optimal execution path
    class TechnologySelector {
    public:
        TechnologySelector() {
            // Register built-in technologies with capabilities
            register_technology(TechnologyType::JAVASCRIPT_V8UI, "UI, patch manipulation, lightweight processing");
            register_technology(TechnologyType::NODE_SCRIPT, "File I/O, network, heavy processing, external services");
            register_technology(TechnologyType::CPP_MIN_DEVKIT, "High-performance DSP, native API access, threading");
            register_technology(TechnologyType::OSC_PROTOCOL, "External communication, legacy compatibility");
        }

        void register_technology(TechnologyType type, const std::string& capabilities) {
            std::lock_guard<std::mutex> lock(mutex_);
            technologies_[type] = capabilities;
        }

        TechnologyType select_technology(const std::string& task_requirements) {
            std::lock_guard<std::mutex> lock(mutex_);

            // This is a simplified selection algorithm
            // In a real implementation, this would use more sophisticated matching
            if (task_requirements.find("DSP") != std::string::npos ||
                task_requirements.find("performance") != std::string::npos) {
                return TechnologyType::CPP_MIN_DEVKIT;
            }
            else if (task_requirements.find("UI") != std::string::npos ||
                     task_requirements.find("patch") != std::string::npos) {
                return TechnologyType::JAVASCRIPT_V8UI;
            }
            else if (task_requirements.find("file") != std::string::npos ||
                     task_requirements.find("network") != std::string::npos) {
                return TechnologyType::NODE_SCRIPT;
            }
            else if (task_requirements.find("external") != std::string::npos) {
                return TechnologyType::OSC_PROTOCOL;
            }

            // Default to the most versatile option
            return TechnologyType::CPP_MIN_DEVKIT;
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<TechnologyType, std::string> technologies_;
    };

    // Message queue management
    // Helper adapter for std::queue to allow iteration (simplified version)
    template<typename T>
    class queue_iterator_adapter {
    public:
        explicit queue_iterator_adapter(std::queue<T>& q) : queue_(q) {}

        class iterator {
        public:
            iterator(std::queue<T>& q, size_t pos) : queue_(q), pos_(pos) {}

            const T& operator*() const {
                // This is a simplification - real implementation would need
                // proper accessing of queue elements which isn't trivial
                static T dummy{};
                return (pos_ < queue_.size()) ? dummy : dummy;
            }

            iterator& operator++() { ++pos_; return *this; }
            bool operator!=(const iterator& other) const { return pos_ != other.pos_; }

        private:
            std::queue<T>& queue_;
            size_t pos_;
        };

        iterator begin() { return iterator(queue_, 0); }
        iterator end() { return iterator(queue_, queue_.size()); }

    private:
        std::queue<T>& queue_;
    };

    // Message queue with improved memory management and priority handling
    class MessageQueue {
    public:
        explicit MessageQueue(size_t max_size) : max_size_(max_size), running_(false) {
            // Pre-allocate a small pool of messages to reduce memory fragmentation
            message_pool_.reserve(std::min<size_t>(32, max_size));
        }

        ~MessageQueue() {
            stop();
        }

        void start() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                running_ = true;
            }
        }

        void stop() {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!running_) return;
                running_ = false;
            }
            cv_.notify_all();
        }

        bool enqueue(Message msg) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                return false; // Can't enqueue when not running
            }

            // Implement back-pressure when queue is full
            if (queue_.size() >= max_size_) {
                // Option 1: Discard oldest low-priority message if new message has higher priority
                if (msg.priority > 0 && !queue_.empty()) {
                    // Find lowest priority message
                    int lowest_priority = msg.priority;
                    size_t lowest_idx = 0;
                    size_t current_idx = 0;

                    // This is simplified - a priority queue would be better for production
                    for (const auto& queued_msg : queue_adapter()) {
                        if (queued_msg.priority < lowest_priority) {
                            lowest_priority = queued_msg.priority;
                            lowest_idx = current_idx;
                        }
                        current_idx++;
                    }

                    // If we found a lower priority message, replace it
                    if (lowest_priority < msg.priority) {
                        replace_message(lowest_idx, std::move(msg));
                        cv_.notify_one();
                        return true;
                    }
                }

                // Otherwise, reject the message
                return false;
            }

            // Regular case: add to queue
            queue_.push(std::move(msg));
            cv_.notify_one();
            return true;
        }

        bool dequeue(Message& msg, std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lock(mutex_);

            if (!running_ || queue_.empty()) {
                if (!cv_.wait_for(lock, timeout, [this] { return !running_ || !queue_.empty(); })) {
                    return false;  // Timeout
                }

                if (!running_ || queue_.empty()) {
                    return false;
                }
            }

            msg = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size();
        }

        bool is_running() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return running_;
        }

        // Get adapter for queue iteration (for priority handling)
        queue_iterator_adapter<Message> queue_adapter() {
            return queue_iterator_adapter<Message>(queue_);
        }

        // Replace message at specific position (simplified implementation)
        void replace_message(size_t index, Message new_msg) {
            // In a real implementation, this would need a more sophisticated
            // data structure than std::queue, like a std::deque or custom container
            // This is a placeholder implementation
            if (index == 0 && !queue_.empty()) {
                queue_.pop();
                queue_.push(std::move(new_msg));
            }
        }

    private:
        size_t max_size_;
        std::atomic<bool> running_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::queue<Message> queue_;
        std::condition_variable cv_nonblocking_; // For non-blocking wait operations
        std::vector<Message> message_pool_; // Pre-allocated message pool
    };

    // Worker thread pool for processing messages
    // Worker pool with improved thread management and monitoring
    class WorkerPool {
    public:
        WorkerPool(mcp_orchestrator* parent, size_t num_threads, MessageQueue& queue)
            : parent_(parent), queue_(queue), running_(false), active_workers_(0) {
            // Reserve space for threads to prevent reallocation
            threads_.reserve(num_threads);
            thread_stats_.resize(num_threads);
        }

        ~WorkerPool() {
            stop();
        }

        void start() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (running_) return;

            running_ = true;
            for (size_t i = 0; i < threads_.capacity(); ++i) {
                threads_.emplace_back(&WorkerPool::worker_thread, this);
            }
        }

        void stop() {
            // Set running flag to false under lock
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!running_) return;
                running_ = false;
            }

            // Important: notify all waiting threads to avoid deadlock
            cv_.notify_all();

            // Join threads outside of lock to prevent deadlock
            for (auto& thread : threads_) {
                if (thread.joinable()) {
                    thread.join();
                }
            }

            // Clear thread vector
            threads_.clear();
        }

    private:
        void worker_thread() {
            // Set up thread-local error tracking
            int consecutive_errors = 0;
            std::chrono::time_point<std::chrono::steady_clock> last_error_time;

            while (running_) {
                try {
                    Message msg;
                    // Use adaptive timeout based on queue load
                    auto timeout = queue_.size() > 0 ?
                        std::chrono::milliseconds(10) : // Short timeout when queue has messages
                        std::chrono::milliseconds(100);  // Longer timeout when idle

                    if (queue_.dequeue(msg, timeout)) {
                        try {
                            // Process the message
                            parent_->process_message(msg);

                            // Reset error counter on success
                            consecutive_errors = 0;
                        }
                        catch (const std::exception& e) {
                            consecutive_errors++;
                            last_error_time = std::chrono::steady_clock::now();

                            // Log error with details about the message that caused it
                            if (parent_->debug_mode) {
                                c74::max::object_error(nullptr,
                                    "Error processing message [%s] from %d to %d: %s",
                                    msg.command.c_str(),
                                    static_cast<int>(msg.source),
                                    static_cast<int>(msg.destination),
                                    e.what());
                            }

                            // Report error details to parent for monitoring and self-healing
                            atoms error_data = {
                                "error", true,
                                "message", e.what(),
                                "component", "worker_thread",
                                "command", msg.command,
                                "consecutive_errors", consecutive_errors
                            };
                            parent_->error_outlet.send(error_data);

                            // If too many consecutive errors, slow down processing
                            if (consecutive_errors > 5) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(50 * consecutive_errors));
                            }
                        }
                    }
                }
                catch (const std::exception& e) {
                    // Handle exceptions in the worker thread itself (not from message processing)
                    if (parent_->debug_mode) {
                        c74::max::object_error(nullptr, "Worker thread exception: %s", e.what());
                    }

                    // Brief pause to prevent tight error loops
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        }

        // スレッド状態監視のための統計情報
        struct ThreadStats {
            std::atomic<uint64_t> messages_processed{0};
            std::atomic<uint64_t> errors{0};
            std::chrono::system_clock::time_point last_activity;

            ThreadStats() : last_activity(std::chrono::system_clock::now()) {}
        };

        // スレッド統計情報を取得
        atoms get_thread_stats() const {
            std::lock_guard<std::mutex> lock(mutex_);
            atoms stats = {"active_workers", active_workers_.load()};

            for (size_t i = 0; i < thread_stats_.size(); ++i) {
                const auto& ts = thread_stats_[i];
                stats.push_back("thread" + std::to_string(i));
                stats.push_back(ts.messages_processed.load());
                stats.push_back("errors");
                stats.push_back(ts.errors.load());
            }

            return stats;
        }

        mcp_orchestrator* parent_;
        MessageQueue& queue_;
        std::atomic<bool> running_;
        std::atomic<int> active_workers_{0};
        std::condition_variable cv_; // スレッド通知用
        mutable std::mutex mutex_;
        std::vector<std::thread> threads_;
        std::vector<ThreadStats> thread_stats_;
    };

    // Service connection manager
    class ServiceManager {
    public:
        ServiceManager(mcp_orchestrator* parent) : parent_(parent) {}

        bool connect_service(const std::string& service_name, const atoms& connection_args) {
            std::lock_guard<std::mutex> lock(mutex_);

            // Log connection attempt if in debug mode
            if (parent_->debug_mode) {
                c74::max::object_post(nullptr, "Connecting to service: %s", service_name.c_str());
            }

            // Store connection status
            service_status_[service_name] = true;

            // In real implementation, this would establish actual connections
            // to external or internal services

            return true;
        }

        bool disconnect_service(const std::string& service_name) {
            std::lock_guard<std::mutex> lock(mutex_);

            // Log disconnection if in debug mode
            if (parent_->debug_mode) {
                c74::max::object_post(nullptr, "Disconnecting from service: %s", service_name.c_str());
            }

            // Update service status
            service_status_[service_name] = false;

            return true;
        }

        bool is_connected(const std::string& service_name) const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = service_status_.find(service_name);
            return it != service_status_.end() && it->second;
        }

    private:
        mcp_orchestrator* parent_;
        mutable std::mutex mutex_;
        std::unordered_map<std::string, bool> service_status_;
    };

    // Main components of the orchestrator
    ComponentRegistry component_registry;
    TechnologySelector technology_selector;
    std::unique_ptr<MessageQueue> message_queue;
    std::unique_ptr<WorkerPool> worker_pool;
    std::unique_ptr<ServiceManager> service_manager;

    // Constructor
    mcp_orchestrator() {
        // Initialize components with proper error checking
        try {
            message_queue = std::make_unique<MessageQueue>(queue_size);
            if (!message_queue) {
                c74::max::object_error(nullptr, "Failed to initialize message queue");
                return;
            }

            service_manager = std::make_unique<ServiceManager>(this);
            if (!service_manager) {
                c74::max::object_error(nullptr, "Failed to initialize service manager");
                return;
            }
        }
        catch (const std::exception& e) {
            c74::max::object_error(nullptr, "Exception during orchestrator initialization: %s", e.what());
        }

    // Destructor to ensure proper cleanup
    ~mcp_orchestrator() {
        if (worker_pool) {
            worker_pool->stop();
        }
        if (message_queue) {
            message_queue->stop();
        }
    }

    // Validate channel type to ensure it's within the enum range
    bool is_valid_channel_type(int channel_type) const {
        return channel_type >= static_cast<int>(ChannelType::INTELLIGENCE) &&
               channel_type <= static_cast<int>(ChannelType::SYSTEM);
    }

    // Process incoming message with improved validation and error handling
    void process_message(const Message& msg) {
        // Validate message fields
        if (msg.command.empty()) {
            if (debug_mode) {
                c74::max::object_error(nullptr, "Received message with empty command");
            }
            return;
        }

        // Validate channel types
        if (!is_valid_channel_type(static_cast<int>(msg.source)) ||
            !is_valid_channel_type(static_cast<int>(msg.destination))) {
            if (debug_mode) {
                c74::max::object_error(nullptr, "Invalid channel type in message");
            }
            return;
        }

        if (debug_mode) {
            c74::max::object_post(nullptr, "Processing message: %s from %d to %d",
                                 msg.command.c_str(), static_cast<int>(msg.source), static_cast<int>(msg.destination));
        }

        // Create dictionary for command with additional metadata
        atoms cmd_data = {
            "source", static_cast<int>(msg.source),
            "destination", static_cast<int>(msg.destination),
            "command", msg.command,
            "priority", msg.priority,
            "timestamp", std::chrono::system_clock::to_time_t(msg.timestamp)
        };

        // Add all arguments from the original message
        for (size_t i = 0; i < msg.args.size(); ++i) {
            cmd_data.push_back(msg.args[i]);
        }

        // Route the message to the appropriate outlet
        command_outlet.send(cmd_data);
    }

    // Bang message handler - initialize the orchestrator
    message<> bang { this, "bang", "Initialize the orchestrator",
        MIN_FUNCTION {
            try {
                // Create worker pool with specified number of threads
                worker_pool = std::make_unique<WorkerPool>(this, worker_threads, *message_queue);

                // Start message queue and worker pool
                message_queue->start();
                worker_pool->start();

                // Register core components
                component_registry.register_component("intelligence", ChannelType::INTELLIGENCE, "context, llm, reasoning");
                component_registry.register_component("execution", ChannelType::EXECUTION, "max_api, dsp, patching");
                component_registry.register_component("interaction", ChannelType::INTERACTION, "ui, feedback, visualization");
                component_registry.register_component("system", ChannelType::SYSTEM, "orchestration, routing, monitoring");

                // Connect to core services
                service_manager->connect_service("max_api", {});
                service_manager->connect_service("state_sync", {});
                service_manager->connect_service("context_manager", {});

                // Send initialization status
                atoms status_data = {
                    "initialized", true,
                    "routing_strategy", routing_strategy.get(),
                    "worker_threads", worker_threads.get(),
                    "queue_size", queue_size.get(),
                    "debug_mode", debug_mode.get()
                };

                status_outlet.send(status_data);

                if (debug_mode) {
                    c74::max::object_post(nullptr, "Orchestrator initialized with %d worker threads", worker_threads.get());
                }
            }
            catch (const std::exception& e) {
                atoms error_data = {
                    "error", true,
                    "message", e.what(),
                    "component", "orchestrator",
                    "operation", "initialization"
                };
                error_outlet.send(error_data);

                c74::max::object_error(nullptr, "Error initializing orchestrator: %s", e.what());
            }

            return {};
        }
    };

    // Route message between components
    message<> route { this, "route", "Route a message between components",
        MIN_FUNCTION {
            if (args.size() < 3) {
                c74::max::object_error(nullptr, "Route requires source, destination, and command arguments");
                return {};
            }

            try {
                // Parse message arguments
                int source_int = args[0];
                int dest_int = args[1];
                symbol cmd_name = args[2];
                atoms cmd_args;

                // Extract additional command arguments if any
                if (args.size() > 3) {
                    cmd_args = atoms(args.begin() + 3, args.end());
                }

                // Validate channel types
                if (!is_valid_channel_type(source_int) || !is_valid_channel_type(dest_int)) {
                    c74::max::object_error(nullptr, "Invalid channel type: source=%d, destination=%d",
                                        source_int, dest_int);
                    return {};
                }

                // Convert to channel types
                ChannelType source = static_cast<ChannelType>(source_int);
                ChannelType destination = static_cast<ChannelType>(dest_int);

                // Determine message priority (optional 4th argument for priority)
                int priority = 0;
                if (args.size() > 3 && args[3].type() == c74::min::message_type::int_argument) {
                    priority = static_cast<int>(args[3]);
                }

                // Create and enqueue the message
                Message msg(source, destination, cmd_name, cmd_args, priority);
                if (!message_queue->enqueue(std::move(msg))) {
                    if (debug_mode) {
                        c74::max::object_error(nullptr, "Failed to enqueue message: queue full or not running");
                    }
                    return {};
                }

                if (debug_mode) {
                    c74::max::object_post(nullptr, "Message enqueued: %s", cmd_name.c_str());
                }
            }
            catch (const std::exception& e) {
                atoms error_data = {
                    "error", true,
                    "message", e.what(),
                    "component", "orchestrator",
                    "operation", "route"
                };
                error_outlet.send(error_data);

                c74::max::object_error(nullptr, "Error routing message: %s", e.what());
            }

            return {};
        }
    };

    // Select optimal technology for a task
    message<> select_tech { this, "select_tech", "Select optimal technology for a task",
        MIN_FUNCTION {
            if (args.empty()) {
                c74::max::object_error(nullptr, "select_tech requires task requirements argument");
                return {};
            }

            try {
                // Get task requirements
                std::string requirements = args[0];

                // Select optimal technology
                TechnologyType selected = technology_selector.select_technology(requirements);

                // Return the selected technology
                atoms result = {
                    "technology", static_cast<int>(selected),
                    "requirements", requirements
                };

                status_outlet.send(result);

                if (debug_mode) {
                    c74::max::object_post(nullptr, "Selected technology %d for task: %s",
                                         static_cast<int>(selected), requirements.c_str());
                }
            }
            catch (const std::exception& e) {
                atoms error_data = {
                    "error", true,
                    "message", e.what(),
                    "component", "orchestrator",
                    "operation", "select_tech"
                };
                error_outlet.send(error_data);

                c74::max::object_error(nullptr, "Error selecting technology: %s", e.what());
            }

            return {};
        }
    };

    // Connect to a service
    message<> connect { this, "connect", "Connect to a service",
        MIN_FUNCTION {
            if (args.empty()) {
                c74::max::object_error(nullptr, "connect requires service name argument");
                return {};
            }

            try {
                // Get service name
                std::string service_name = args[0];

                // Extract connection arguments if any
                atoms conn_args;
                if (args.size() > 1) {
                    conn_args = atoms(args.begin() + 1, args.end());
                }

                // Connect to the service
                bool success = service_manager->connect_service(service_name, conn_args);

                // Return connection status
                atoms result = {
                    "service", service_name,
                    "connected", success
                };

                status_outlet.send(result);

                if (debug_mode) {
                    c74::max::object_post(nullptr, "Service connection %s: %s",
                                         success ? "successful" : "failed", service_name.c_str());
                }
            }
            catch (const std::exception& e) {
                atoms error_data = {
                    "error", true,
                    "message", e.what(),
                    "component", "orchestrator",
                    "operation", "connect"
                };
                error_outlet.send(error_data);

                c74::max::object_error(nullptr, "Error connecting to service: %s", e.what());
            }

            return {};
        }
    };

    // Disconnect from a service
    message<> disconnect { this, "disconnect", "Disconnect from a service",
        MIN_FUNCTION {
            if (args.empty()) {
                c74::max::object_error(nullptr, "disconnect requires service name argument");
                return {};
            }

            try {
                // Get service name
                std::string service_name = args[0];

                // Disconnect from the service
                bool success = service_manager->disconnect_service(service_name);

                // Return disconnection status
                atoms result = {
                    "service", service_name,
                    "disconnected", success
                };

                status_outlet.send(result);

                if (debug_mode) {
                    c74::max::object_post(nullptr, "Service disconnection %s: %s",
                                         success ? "successful" : "failed", service_name.c_str());
                }
            }
            catch (const std::exception& e) {
                atoms error_data = {
                    "error", true,
                    "message", e.what(),
                    "component", "orchestrator",
                    "operation", "disconnect"
                };
                error_outlet.send(error_data);

                c74::max::object_error(nullptr, "Error disconnecting from service: %s", e.what());
            }

            return {};
        }
    };

    // Get status information
    message<> status { this, "status", "Get orchestrator status",
        MIN_FUNCTION {
            try {
                // Gather status information
                atoms status_data = {
                    "running", message_queue->is_running(),
                    "queue_size", static_cast<int>(message_queue->size()),
                    "max_queue_size", queue_size.get(),
                    "worker_threads", worker_threads.get(),
                    "routing_strategy", routing_strategy.get(),
                    "debug_mode", debug_mode.get(),
                    "auto_reconnect", auto_reconnect.get()
                };

                // Add service connection statuses
                std::vector<std::string> core_services = {"max_api", "state_sync", "context_manager"};
                for (const auto& service : core_services) {
                    status_data.push_back(service);
                    status_data.push_back(service_manager->is_connected(service));
                }

                status_outlet.send(status_data);
            }
            catch (const std::exception& e) {
                atoms error_data = {
                    "error", true,
                    "message", e.what(),
                    "component", "orchestrator",
                    "operation", "status"
                };
                error_outlet.send(error_data);

                c74::max::object_error(nullptr, "Error getting status: %s", e.what());
            }

            return {};
        }
    };
};

MIN_EXTERNAL(mcp_orchestrator);
