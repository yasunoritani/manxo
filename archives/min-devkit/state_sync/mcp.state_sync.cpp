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
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace c74::min;
using json = nlohmann::json;
namespace fs = std::filesystem;

/**
 * @brief State Synchronization Engine for Max 9-Claude Desktop
 * 
 * This module implements the state synchronization part of the integration layer:
 * - Maintains model of Max 9 state (Session, Patches, Objects, Parameters)
 * - Provides event-based synchronization
 * - Implements differential state synchronization
 * - Handles state persistence and restoration
 * - Manages conflict resolution
 * 
 * It works in conjunction with the orchestrator to provide a coherent state model:
 */
class mcp_state_sync : public object<mcp_state_sync> {
public:
    MIN_DESCRIPTION {"Native State Synchronization Engine for Max 9-Claude Desktop"};
    MIN_TAGS {"mcp-bridge"};
    MIN_AUTHOR {"Max 9-Claude Integration Team"};
    MIN_RELATED {"mcp.orchestrator, mcp.core, mcp.ui, mcp.context"};

    // Inlets and outlets
    inlet<> main_inlet { this, "(bang) initialize state engine, (dictionary) process state change" };
    outlet<> sync_outlet { this, "(dictionary) state change notifications" };
    outlet<> response_outlet { this, "(dictionary) sync responses" };
    outlet<> error_outlet { this, "(dictionary) error information" };
    outlet<> orchestrator_outlet { this, "(dictionary) messages to orchestrator" };

    // Core configuration attributes
    attribute<bool> debug_mode { this, "debug", false,
        description {"Enable debug mode for detailed logging"}
    };
    
    attribute<symbol> sync_strategy { this, "strategy", "timestamp",
        description {"Conflict resolution strategy: 'timestamp' or 'priority'"}
    };
    
    attribute<symbol> storage_path { this, "storage", "~/Documents/Max 9/state",
        description {"Path for storing persistent state"}
    };
    
    attribute<number> sync_interval { this, "interval", 500,
        description {"Differential sync interval in milliseconds"}
    };
    
    attribute<bool> auto_persist { this, "autopersist", false,
        description {"Automatically persist state changes"}
    };

    // Forward declarations of state model classes
    class Parameter;
    class MaxObject;
    class Connection;
    class Patch;
    class Session;
    class StateEvent;
    class StateChange;
    class StateDiff;

    // Constructor
    mcp_state_sync(const atoms& args = {}) {
        if (debug_mode)
            cout << "mcp.state_sync: initializing state synchronization engine" << endl;
            
        // Initialize state model with a default session
        is_initialized = false;
        is_sync_running = false;
        
        // Set last sync time to current time
        last_sync_time = std::chrono::system_clock::now();
    }
    
    // Destructor with cleanup
    ~mcp_state_sync() {
        stop_sync_thread();
        
        if (debug_mode)
            cout << "mcp.state_sync: state synchronization engine shutdown" << endl;
    }

    // Initialize state engine
    message<> init { this, "init", "Initialize state synchronization engine",
        MIN_FUNCTION {
            initialize();
            
            // Register with orchestrator
            register_with_orchestrator();
            
            return {};
        }
    };
    
    // Bang message handler
    message<> bang { this, "bang", "Initialize or get current state snapshot",
        MIN_FUNCTION {
            if (!is_initialized) {
                initialize();
            } else {
                send_state_snapshot();
            }
            return {};
        }
    };
    
    // Process state change
    message<> state_change { this, "state_change", "Process state change event",
        MIN_FUNCTION {
            if (args.size() < 3) {
                error("state_change requires: category, event_type, object_id [data]");
                return {};
            }
            
            symbol category = args[0];
            symbol event_type = args[1];
            symbol object_id = args[2];
            
            // Create JSON from remaining args if any
            json change_data = json::object();
            if (args.size() > 3) {
                try {
                    // Parse the JSON string from args
                    atom json_data = args[3];
                    if (json_data.type() == message_type::symbol) {
                        change_data = json::parse(std::string(json_data));
                    }
                }
                catch (const std::exception& e) {
                    error("Failed to parse change data: " + std::string(e.what()));
                    return {};
                }
            }
            
            // Process the state change
            process_state_change(category, event_type, object_id, change_data);
            return {};
        }
    };
    
    // Sync request handler
    message<> sync_request { this, "sync_request", "Handle synchronization request",
        MIN_FUNCTION {
            if (args.empty()) {
                error("sync_request requires at least request_id");
                return {};
            }
            
            symbol request_id = args[0];
            symbol category;
            symbol target_id;
            
            if (args.size() > 1) {
                category = args[1];
            }
            
            if (args.size() > 2) {
                target_id = args[2];
            }
            
            handle_sync_request(request_id, category, target_id);
            return {};
        }
    };
    
    // Differential sync request
    message<> diff_sync { this, "diff_sync", "Handle differential sync request",
        MIN_FUNCTION {
            if (args.size() < 2) {
                error("diff_sync requires: request_id last_sync_time");
                return {};
            }
            
            symbol request_id = args[0];
            long long last_sync_time = args[1];
            
            handle_diff_sync(request_id, last_sync_time);
            return {};
        }
    };
    
    // Save state
    message<> save_state { this, "save_state", "Save current state to file",
        MIN_FUNCTION {
            if (args.empty()) {
                error("save_state requires at least request_id");
                return {};
            }
            
            symbol request_id = args[0];
            symbol path;
            
            if (args.size() > 1) {
                path = args[1];
            }
            
            save_current_state(request_id, path);
            return {};
        }
    };
    
    // Load state
    message<> load_state { this, "load_state", "Load state from file",
        MIN_FUNCTION {
            if (args.size() < 2) {
                error("load_state requires: request_id file_path");
                return {};
            }
            
            symbol request_id = args[0];
            symbol path = args[1];
            
            load_state_from_file(request_id, path);
            return {};
        }
    };

private:
    // Core state objects
    std::unique_ptr<Session> current_session;
    std::mutex state_mutex;
    std::atomic<bool> is_initialized{false};
    std::atomic<bool> is_sync_running{false};
    std::thread sync_thread;
    std::condition_variable sync_cv;
    
    // Event history for diff sync
    std::vector<StateEvent> event_history;
    std::mutex history_mutex;
    std::chrono::system_clock::time_point last_sync_time;
    
    // Helper functions
    void initialize();
    void start_sync_thread();
    void stop_sync_thread();
    void sync_thread_function();
    void process_state_change(const symbol& category, const symbol& event_type, 
                            const symbol& object_id, const json& change_data);
    
    // Category-specific state change processing methods
    void process_session_change(const mcp::state::StateEvent::EventType& event_type,
                              const std::string& session_id, const json& change_data);
    void process_patch_change(const mcp::state::StateEvent::EventType& event_type,
                            const std::string& patch_id, const json& change_data);
    void process_object_change(const mcp::state::StateEvent::EventType& event_type,
                             const std::string& object_id, const json& change_data);
    void process_parameter_change(const mcp::state::StateEvent::EventType& event_type,
                                const std::string& param_id, const json& change_data);
    void process_connection_change(const mcp::state::StateEvent::EventType& event_type,
                                 const std::string& connection_id, const json& change_data);
    void process_global_setting_change(const mcp::state::StateEvent::EventType& event_type,
                                     const std::string& setting_id, const json& change_data);
    
    void handle_sync_request(const symbol& request_id, const symbol& category, 
                           const symbol& target_id);
    // Differential synchronization based on timestamp
void handle_diff_sync(const symbol& request_id, long long last_sync_timestamp);
    void save_current_state(const symbol& request_id, const symbol& path);
    void load_state_from_file(const symbol& request_id, const symbol& path);
    void send_state_snapshot();
    void notify_state_change(const StateChange& change);
    // Compute differences between two state snapshots
std::vector<mcp::state::StateDiff> compute_state_diff(const json& base_state, 
                                            const json& current_state);
    json resolve_conflicts(const json& local_state, const json& remote_state);
    json resolve_conflicts_by_timestamp(const json& local_state, const json& remote_state);
    json resolve_conflicts_by_priority(const json& local_state, const json& remote_state);
    void error(const std::string& message);
    
    // Orchestrator integration methods
    void register_with_orchestrator();
    void send_to_orchestrator(const std::string& command, const c74::min::dict& data);
    
    // Orchestrator message handler
    message<> from_orchestrator { this, "from_orchestrator", "Handle message from orchestrator",
        MIN_FUNCTION {
            handle_orchestrator_message(args);
            return {};
        }
    };
};

// Implementation of helper methods

void mcp_state_sync::register_with_orchestrator() {
    try {
        // Create registration message
        c74::min::dict registration;
        registration["command"] = "register";
        registration["source"] = "state_sync";
        registration["type"] = "service";
        registration["capabilities"] = {"state_management", "state_sync", "state_persistence"};
        
        // Send to orchestrator
        orchestrator_outlet.send("to_orchestrator", registration);
        
        if (debug_mode) {
            cout << "mcp.state_sync: registered with orchestrator" << endl;
        }
    } catch (const std::exception& e) {
        error(std::string("Failed to register with orchestrator: ") + e.what());
    }
}

void mcp_state_sync::send_to_orchestrator(const std::string& command, const c74::min::dict& data) {
    try {
        // Create message dictionary
        c74::min::dict message = data;
        message["command"] = command;
        message["source"] = "state_sync";
        message["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        // Send to orchestrator
        orchestrator_outlet.send("to_orchestrator", message);
        
        if (debug_mode) {
            cout << "mcp.state_sync: sent '" << command << "' to orchestrator" << endl;
        }
    } catch (const std::exception& e) {
        error(std::string("Failed to send to orchestrator: ") + e.what());
    }
}

void mcp_state_sync::handle_orchestrator_message(const atoms& args) {
    if (args.empty()) {
        error("Received empty message from orchestrator");
        return;
    }
    
    try {
        // Extract message from args
        c74::min::dict message(args[0]);
        
        // Get command
        if (!message.contains("command")) {
            error("Message from orchestrator missing 'command' field");
            return;
        }
        
        std::string command = message["command"];
        
        if (debug_mode) {
            cout << "mcp.state_sync: received '" << command << "' from orchestrator" << endl;
        }
        
        // Process command
        if (command == "state_request") {
            // Request for state information
            if (!message.contains("category") || !message.contains("requestId")) {
                error("Incomplete state request from orchestrator");
                return;
            }
            
            // Extract parameters
            symbol request_id = message["requestId"];
            symbol category = message.contains("category") ? symbol(message["category"]) : symbol("");
            symbol target_id = message.contains("targetId") ? symbol(message["targetId"]) : symbol("");
            
            // Handle the request
            handle_sync_request(request_id, category, target_id);
            
        } else if (command == "state_change") {
            // State change notification
            if (!message.contains("category") || !message.contains("eventType") || 
                !message.contains("objectId") || !message.contains("data")) {
                error("Incomplete state change notification from orchestrator");
                return;
            }
            
            // Extract parameters
            symbol category = message["category"];
            symbol event_type = message["eventType"];
            symbol object_id = message["objectId"];
            json change_data = json::parse(std::string(message["data"]));
            
            // Process the state change
            process_state_change(category, event_type, object_id, change_data);
            
        } else if (command == "diff_sync") {
            // Differential sync request
            if (!message.contains("requestId") || !message.contains("lastSyncTimestamp")) {
                error("Incomplete diff sync request from orchestrator");
                return;
            }
            
            // Extract parameters
            symbol request_id = message["requestId"];
            long long last_sync_timestamp = message["lastSyncTimestamp"];
            
            // Handle diff sync request
            handle_diff_sync(request_id, last_sync_timestamp);
            
        } else if (command == "save_state") {
            // Save state request
            if (!message.contains("requestId") || !message.contains("path")) {
                error("Incomplete save state request from orchestrator");
                return;
            }
            
            // Extract parameters
            symbol request_id = message["requestId"];
            symbol path = message["path"];
            
            // Handle save request
            save_current_state(request_id, path);
            
        } else if (command == "load_state") {
            // Load state request
            if (!message.contains("requestId") || !message.contains("path")) {
                error("Incomplete load state request from orchestrator");
                return;
            }
            
            // Extract parameters
            symbol request_id = message["requestId"];
            symbol path = message["path"];
            
            // Handle load request
            load_state_from_file(request_id, path);
            
        } else if (command == "ping") {
            // Ping - respond with pong
            c74::min::dict pong;
            pong["command"] = "pong";
            pong["requestId"] = message.contains("requestId") ? message["requestId"] : "";
            send_to_orchestrator("pong", pong);
            
        } else {
            error("Unknown command from orchestrator: " + command);
        }
        
    } catch (const std::exception& e) {
        error(std::string("Error processing orchestrator message: ") + e.what());
    }
}

void mcp_state_sync::initialize() {
    std::lock_guard<std::mutex> lock(state_mutex);
    
    if (debug_mode)
        cout << "mcp.state_sync: initializing state engine" << endl;
    
    // Create a new session with a unique ID
    std::string session_id = "session-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    current_session = std::make_unique<mcp::state::Session>(session_id, "Max 9 Session");
    
    // Initialize default global settings
    current_session->get_global_settings().set_setting("oscPort", 7400);
    current_session->get_global_settings().set_setting("sampling_rate", 44100);
    
    // Reset event history
    {
        std::lock_guard<std::mutex> history_lock(history_mutex);
        event_history.clear();
        last_sync_time = std::chrono::system_clock::now();
    }
    
    // Start the sync thread if needed
    if (sync_interval > 0) {
        start_sync_thread();
    }
    
    is_initialized = true;
    
    // Notify initialization complete
    c74::min::dict init_data;
    init_data["status"] = "initialized";
    init_data["sessionId"] = session_id;
    response_outlet.send("init_complete", init_data);
}

void mcp_state_sync::start_sync_thread() {
    if (is_sync_running)
        return;
    
    is_sync_running = true;
    sync_thread = std::thread(&mcp_state_sync::sync_thread_function, this);
    
    if (debug_mode)
        cout << "mcp.state_sync: sync thread started" << endl;
}

void mcp_state_sync::stop_sync_thread() {
    if (!is_sync_running)
        return;
    
    is_sync_running = false;
    sync_cv.notify_all();
    
    if (sync_thread.joinable()) {
        sync_thread.join();
    }
    
    if (debug_mode)
        cout << "mcp.state_sync: sync thread stopped" << endl;
}

void mcp_state_sync::sync_thread_function() {
    while (is_sync_running) {
        // Wait for the specified interval
        {
            std::unique_lock<std::mutex> lock(state_mutex);
            sync_cv.wait_for(lock, std::chrono::milliseconds(sync_interval), 
                            [this]() { return !is_sync_running; });
            
            // Exit if thread was signaled to stop
            if (!is_sync_running)
                break;
        }
        
        // Auto-persist if enabled
        if (auto_persist && is_initialized) {
            try {
                symbol auto_req_id = "auto-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                save_current_state(auto_req_id, "");
            }
            catch (const std::exception& e) {
                error(std::string("Auto-persist failed: ") + e.what());
            }
        }
    }
}

void mcp_state_sync::error(const std::string& message) {
    c74::min::dict error_data;
    error_data["error"] = message;
    error_data["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    
    error_outlet.send("error", error_data);
    
    if (debug_mode)
        cout << "mcp.state_sync ERROR: " << message << endl;
}

void mcp_state_sync::process_state_change(const symbol& category, const symbol& event_type, 
                                        const symbol& object_id, const json& change_data) {
    if (!is_initialized) {
        error("State engine not initialized");
        return;
    }
    
    if (debug_mode) {
        cout << "mcp.state_sync: processing state change - " 
             << category << "/" << event_type << " for " << object_id << endl;
    }
    
    try {
        // Convert string category and event type to enum values
        mcp::state::StateEvent::Category cat = mcp::state::StateEvent::string_to_category(category);
        mcp::state::StateEvent::EventType evt = mcp::state::StateEvent::string_to_event_type(event_type);
        
        // Create state event
        mcp::state::StateEvent event(cat, evt, object_id, change_data);
        
        // Apply the state change
        std::lock_guard<std::mutex> lock(state_mutex);
        
        if (!current_session) {
            error("No active session");
            return;
        }
        
        // Apply change based on category and event type
        switch (cat) {
            case mcp::state::StateEvent::Category::Session:
                process_session_change(evt, object_id, change_data);
                break;
                
            case mcp::state::StateEvent::Category::Patch:
                process_patch_change(evt, object_id, change_data);
                break;
                
            case mcp::state::StateEvent::Category::Object:
                process_object_change(evt, object_id, change_data);
                break;
                
            case mcp::state::StateEvent::Category::Parameter:
                process_parameter_change(evt, object_id, change_data);
                break;
                
            case mcp::state::StateEvent::Category::Connection:
                process_connection_change(evt, object_id, change_data);
                break;
                
            case mcp::state::StateEvent::Category::GlobalSetting:
                process_global_setting_change(evt, object_id, change_data);
                break;
                
            default:
                error("Unknown category: " + std::string(category));
                return;
        }
        
        // Add to event history
        {
            std::lock_guard<std::mutex> history_lock(history_mutex);
            event_history.push_back(event);
            
            // Limit history size (keep last 1000 events)
            if (event_history.size() > 1000) {
                event_history.erase(event_history.begin());
            }
        }
        
        // Notify observers about the state change
        mcp::state::StateChange change(event);
        notify_state_change(change);
        
    } catch (const std::exception& e) {
        error(std::string("Failed to process state change: ") + e.what());
    }
}

void mcp_state_sync::send_state_snapshot() {
    if (!is_initialized || !current_session) {
        error("Cannot send state snapshot: State engine not initialized");
        return;
    }
    
    try {
        std::lock_guard<std::mutex> lock(state_mutex);
        
        // Convert session to JSON
        json session_json = current_session->to_json();
        
        // Create a dictionary with the state data
        c74::min::dict snapshot_data;
        snapshot_data["state"] = std::string(session_json.dump());
        snapshot_data["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        // Send state snapshot
        sync_outlet.send("state_snapshot", snapshot_data);
        
        if (debug_mode) {
            cout << "mcp.state_sync: sent state snapshot" << endl;
        }
        
    } catch (const std::exception& e) {
        error(std::string("Failed to send state snapshot: ") + e.what());
    }
}

void mcp_state_sync::notify_state_change(const mcp::state::StateChange& change) {
    // Create notification data
    c74::min::dict notification_data;
    notification_data["category"] = mcp::state::StateEvent::category_to_string(change.get_event().get_category());
    notification_data["eventType"] = mcp::state::StateEvent::event_type_to_string(change.get_event().get_event_type());
    notification_data["objectId"] = change.get_event().get_object_id();
    notification_data["data"] = std::string(change.get_event().get_data().dump());
    notification_data["timestamp"] = change.get_event().get_timestamp();
    
    // Send notification
    sync_outlet.send("state_change", notification_data);
    
    if (debug_mode) {
        cout << "mcp.state_sync: notified state change - " 
             << notification_data["category"] << "/" << notification_data["eventType"] 
             << " for " << notification_data["objectId"] << endl;
    }
}

void mcp_state_sync::handle_sync_request(const symbol& request_id, const symbol& category, 
                                       const symbol& target_id) {
    if (!is_initialized || !current_session) {
        error("Cannot handle sync request: State engine not initialized");
        return;
    }
    
    if (debug_mode) {
        cout << "mcp.state_sync: handling sync request - id=" << request_id 
             << ", category=" << category << ", target=" << target_id << endl;
    }
    
    try {
        std::lock_guard<std::mutex> lock(state_mutex);
        json response_data;
        
        // Convert target_id to string for easier handling
        std::string target_id_str = target_id.empty() ? "" : std::string(target_id);
        
        // Determine what to include in the response based on category and target_id
        if (category.empty()) {
            // Return full session state (complete snapshot)
            response_data = current_session->to_json();
            response_data["type"] = "full_snapshot";
        } else {
            mcp::state::StateEvent::Category cat = mcp::state::StateEvent::string_to_category(category);
            
            switch (cat) {
                case mcp::state::StateEvent::Category::Session:
                    // Return session metadata (without all patches/objects details)
                    response_data["id"] = current_session->get_id();
                    response_data["name"] = current_session->get_name();
                    response_data["creationTime"] = current_session->get_creation_time();
                    response_data["lastModifiedTime"] = current_session->get_last_modified_time();
                    response_data["type"] = "session_metadata";
                    break;
                    
                case mcp::state::StateEvent::Category::Patch:
                    if (target_id.empty() || target_id_str == "all") {
                        // Return all patches
                        response_data["patches"] = json::array();
                        for (const auto& patch_pair : current_session->get_patches()) {
                            response_data["patches"].push_back(patch_pair.second->to_json());
                        }
                        response_data["type"] = "all_patches";
                    } else {
                        // Return specific patch
                        if (current_session->has_patch(target_id_str)) {
                            auto patch = current_session->get_patch(target_id_str);
                            response_data = patch->to_json();
                            response_data["type"] = "patch";
                            
                            // Include all objects and connections in this patch
                            response_data["objects"] = json::array();
                            for (const auto& obj_pair : patch->get_objects()) {
                                response_data["objects"].push_back(obj_pair.second->to_json());
                            }
                            
                            response_data["connections"] = json::array();
                            for (const auto& conn_pair : patch->get_connections()) {
                                response_data["connections"].push_back(conn_pair.second->to_json());
                            }
                        } else {
                            throw std::runtime_error("Patch not found: " + target_id_str);
                        }
                    }
                    break;
                    
                case mcp::state::StateEvent::Category::Object:
                    if (target_id.empty() || target_id_str == "all") {
                        // Iterate through all patches to collect all objects
                        response_data["objects"] = json::array();
                        for (const auto& patch_pair : current_session->get_patches()) {
                            auto patch = patch_pair.second;
                            for (const auto& obj_pair : patch->get_objects()) {
                                json obj_data = obj_pair.second->to_json();
                                obj_data["patchId"] = patch_pair.first;  // Add patch context
                                response_data["objects"].push_back(obj_data);
                            }
                        }
                        response_data["type"] = "all_objects";
                    } else {
                        // Find the specific object across all patches
                        bool found = false;
                        for (const auto& patch_pair : current_session->get_patches()) {
                            auto patch = patch_pair.second;
                            if (patch->has_object(target_id_str)) {
                                auto obj = patch->get_object(target_id_str);
                                response_data = obj->to_json();
                                response_data["patchId"] = patch_pair.first;  // Add patch context
                                response_data["type"] = "object";
                                found = true;
                                break;
                            }
                        }
                        
                        if (!found) {
                            throw std::runtime_error("Object not found: " + target_id_str);
                        }
                    }
                    break;
                    
                case mcp::state::StateEvent::Category::Parameter:
                    if (target_id.empty() || target_id_str == "all") {
                        // This would require iterating through all objects in all patches
                        // which is inefficient - better to request at object level
                        throw std::runtime_error("Cannot sync all parameters at once, request by object instead");
                    } else {
                        // Parameter syncs need more context - parameter name and object id
                        // Assuming target_id is in format "objectId.paramName"
                        size_t dot_pos = target_id_str.find('.');
                        if (dot_pos == std::string::npos || dot_pos == 0 || dot_pos == target_id_str.length() - 1) {
                            throw std::runtime_error("Invalid parameter ID format, expected 'objectId.paramName': " + target_id_str);
                        }
                        
                        std::string object_id = target_id_str.substr(0, dot_pos);
                        std::string param_name = target_id_str.substr(dot_pos + 1);
                        
                        // Find the object that contains this parameter
                        bool found = false;
                        for (const auto& patch_pair : current_session->get_patches()) {
                            auto patch = patch_pair.second;
                            if (patch->has_object(object_id)) {
                                auto obj = patch->get_object(object_id);
                                if (obj->has_parameter(param_name)) {
                                    auto param = obj->get_parameter(param_name);
                                    response_data = param.to_json();
                                    response_data["objectId"] = object_id;
                                    response_data["patchId"] = patch_pair.first;
                                    response_data["type"] = "parameter";
                                    found = true;
                                    break;
                                }
                            }
                        }
                        
                        if (!found) {
                            throw std::runtime_error("Parameter not found: " + target_id_str);
                        }
                    }
                    break;
                    
                case mcp::state::StateEvent::Category::Connection:
                    if (target_id.empty() || target_id_str == "all") {
                        // Iterate through all patches to collect all connections
                        response_data["connections"] = json::array();
                        for (const auto& patch_pair : current_session->get_patches()) {
                            auto patch = patch_pair.second;
                            for (const auto& conn_pair : patch->get_connections()) {
                                json conn_data = conn_pair.second->to_json();
                                conn_data["patchId"] = patch_pair.first;  // Add patch context
                                response_data["connections"].push_back(conn_data);
                            }
                        }
                        response_data["type"] = "all_connections";
                    } else {
                        // Find the specific connection across all patches
                        bool found = false;
                        for (const auto& patch_pair : current_session->get_patches()) {
                            auto patch = patch_pair.second;
                            if (patch->has_connection(target_id_str)) {
                                auto conn = patch->get_connection(target_id_str);
                                response_data = conn->to_json();
                                response_data["patchId"] = patch_pair.first;  // Add patch context
                                response_data["type"] = "connection";
                                found = true;
                                break;
                            }
                        }
                        
                        if (!found) {
                            throw std::runtime_error("Connection not found: " + target_id_str);
                        }
                    }
                    break;
                    
                case mcp::state::StateEvent::Category::GlobalSetting:
                    if (target_id.empty() || target_id_str == "all") {
                        // Return all global settings
                        response_data = current_session->get_global_settings().to_json();
                        response_data["type"] = "all_global_settings";
                    } else {
                        // Return specific global setting
                        auto& settings = current_session->get_global_settings();
                        if (settings.has_setting(target_id_str)) {
                            response_data["name"] = target_id_str;
                            response_data["value"] = settings.get_setting(target_id_str);
                            response_data["type"] = "global_setting";
                        } else {
                            throw std::runtime_error("Global setting not found: " + target_id_str);
                        }
                    }
                    break;
                    
                default:
                    throw std::runtime_error("Unsupported category for sync request: " + std::string(category));
            }
        }
        
        // Create response dictionary
        c74::min::dict response;
        response["requestId"] = request_id;
        response["category"] = category.empty() ? "full" : std::string(category);
        response["targetId"] = target_id;
        response["data"] = std::string(response_data.dump());
        response["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        // Send response
        response_outlet.send("sync_response", response);
        
        if (debug_mode) {
            cout << "mcp.state_sync: sent sync response for request " << request_id << endl;
        }
        
    } catch (const std::exception& e) {
        error(std::string("Failed to handle sync request: ") + e.what());
        
        // Send error response
        c74::min::dict error_response;
        error_response["requestId"] = request_id;
        error_response["category"] = category;
        error_response["targetId"] = target_id;
        error_response["error"] = e.what();
        error_outlet.send("sync_error", error_response);
    }
}

// Category-specific state change processing methods implementation

void mcp_state_sync::process_session_change(const mcp::state::StateEvent::EventType& event_type,
                                          const std::string& session_id, const json& change_data) {
    // Check if this is for our current session
    if (session_id != current_session->get_id()) {
        // Ignore events for other sessions
        if (debug_mode) {
            cout << "mcp.state_sync: ignoring event for different session" << endl;
        }
        return;
    }
    
    switch (event_type) {
        case mcp::state::StateEvent::EventType::Updated:
            // Update session properties
            if (change_data.contains("name")) {
                current_session->set_name(change_data["name"]);
            }
            break;
            
        case mcp::state::StateEvent::EventType::StateChanged:
            // Full state update - replace entire session
            if (change_data.contains("state")) {
                try {
                    // Parse and replace current session
                    json session_json = change_data["state"];
                    current_session = std::make_unique<mcp::state::Session>(
                        mcp::state::Session::from_json(session_json)
                    );
                } catch (const std::exception& e) {
                    error(std::string("Failed to update session state: ") + e.what());
                }
            }
            break;
            
        default:
            // Unsupported event type for session
            error("Unsupported event type for session: " + 
                  mcp::state::StateEvent::event_type_to_string(event_type));
            break;
    }
}

void mcp_state_sync::process_patch_change(const mcp::state::StateEvent::EventType& event_type,
                                        const std::string& patch_id, const json& change_data) {
    switch (event_type) {
        case mcp::state::StateEvent::EventType::Created:
            // Create new patch
            try {
                if (!change_data.contains("name")) {
                    throw std::runtime_error("Missing required field 'name'");
                }
                
                std::string name = change_data["name"];
                std::string path = change_data.value("path", "");
                
                auto patch = std::make_shared<mcp::state::Patch>(patch_id, name, path);
                current_session->add_patch(patch);
                
                if (debug_mode) {
                    cout << "mcp.state_sync: created patch " << name << " (" << patch_id << ")" << endl;
                }
            } catch (const std::exception& e) {
                error(std::string("Failed to create patch: ") + e.what());
            }
            break;
            
        case mcp::state::StateEvent::EventType::Updated:
            // Update existing patch
            try {
                if (!current_session->has_patch(patch_id)) {
                    throw std::runtime_error("Patch not found: " + patch_id);
                }
                
                auto patch = current_session->get_patch(patch_id);
                
                if (change_data.contains("name")) {
                    patch->set_name(change_data["name"]);
                }
                
                if (change_data.contains("path")) {
                    patch->set_path(change_data["path"]);
                }
                
                if (change_data.contains("isModified")) {
                    patch->set_modified(change_data["isModified"]);
                }
            } catch (const std::exception& e) {
                error(std::string("Failed to update patch: ") + e.what());
            }
            break;
            
        case mcp::state::StateEvent::EventType::Deleted:
            // Delete patch
            try {
                if (current_session->has_patch(patch_id)) {
                    current_session->remove_patch(patch_id);
                    
                    if (debug_mode) {
                        cout << "mcp.state_sync: deleted patch " << patch_id << endl;
                    }
                }
            } catch (const std::exception& e) {
                error(std::string("Failed to delete patch: ") + e.what());
            }
            break;
            
        default:
            // Unsupported event type for patch
            error("Unsupported event type for patch: " + 
                  mcp::state::StateEvent::event_type_to_string(event_type));
            break;
    }
}

void mcp_state_sync::process_object_change(const mcp::state::StateEvent::EventType& event_type,
                                         const std::string& object_id, const json& change_data) {
    try {
        // Object changes need to know which patch they belong to
        if (!change_data.contains("patchId")) {
            throw std::runtime_error("Missing required field 'patchId'");
        }
        
        std::string patch_id = change_data["patchId"];
        
        if (!current_session->has_patch(patch_id)) {
            throw std::runtime_error("Patch not found: " + patch_id);
        }
        
        auto patch = current_session->get_patch(patch_id);
        
        switch (event_type) {
            case mcp::state::StateEvent::EventType::Created:
                // Create new object
                if (!change_data.contains("type")) {
                    throw std::runtime_error("Missing required field 'type'");
                }
                
                {
                    std::string obj_type = change_data["type"];
                    auto obj = std::make_shared<mcp::state::MaxObject>(object_id, obj_type);
                    
                    // Set position if provided
                    if (change_data.contains("position")) {
                        auto& pos = change_data["position"];
                        obj->set_position(pos["x"], pos["y"]);
                    }
                    
                    // Set size if provided
                    if (change_data.contains("size")) {
                        auto& size = change_data["size"];
                        obj->set_size(size["width"], size["height"]);
                    }
                    
                    // Set inlets and outlets
                    if (change_data.contains("inlets")) {
                        obj->set_inlets(change_data["inlets"]);
                    }
                    
                    if (change_data.contains("outlets")) {
                        obj->set_outlets(change_data["outlets"]);
                    }
                    
                    // Add parameters if provided
                    if (change_data.contains("parameters") && change_data["parameters"].is_array()) {
                        for (const auto& param_json : change_data["parameters"]) {
                            obj->add_parameter(mcp::state::Parameter::from_json(param_json));
                        }
                    }
                    
                    patch->add_object(obj);
                    
                    if (debug_mode) {
                        cout << "mcp.state_sync: created object " << obj_type << " (" << object_id << ")" << endl;
                    }
                }
                break;
                
            case mcp::state::StateEvent::EventType::Updated:
                // Update existing object
                if (!patch->has_object(object_id)) {
                    throw std::runtime_error("Object not found: " + object_id);
                }
                
                {
                    auto obj = patch->get_object(object_id);
                    
                    // Update position if provided
                    if (change_data.contains("position")) {
                        auto& pos = change_data["position"];
                        obj->set_position(pos["x"], pos["y"]);
                    }
                    
                    // Update size if provided
                    if (change_data.contains("size")) {
                        auto& size = change_data["size"];
                        obj->set_size(size["width"], size["height"]);
                    }
                }
                break;
                
            case mcp::state::StateEvent::EventType::Deleted:
                // Delete object
                if (patch->has_object(object_id)) {
                    patch->remove_object(object_id);
                    
                    if (debug_mode) {
                        cout << "mcp.state_sync: deleted object " << object_id << endl;
                    }
                }
                break;
                
            case mcp::state::StateEvent::EventType::Moved:
                // Move object (special case of update)
                if (!patch->has_object(object_id)) {
                    throw std::runtime_error("Object not found: " + object_id);
                }
                
                if (!change_data.contains("position")) {
                    throw std::runtime_error("Missing position data for move event");
                }
                
                {
                    auto obj = patch->get_object(object_id);
                    auto& pos = change_data["position"];
                    obj->set_position(pos["x"], pos["y"]);
                }
                break;
                
            case mcp::state::StateEvent::EventType::Resized:
                // Resize object (special case of update)
                if (!patch->has_object(object_id)) {
                    throw std::runtime_error("Object not found: " + object_id);
                }
                
                if (!change_data.contains("size")) {
                    throw std::runtime_error("Missing size data for resize event");
                }
                
                {
                    auto obj = patch->get_object(object_id);
                    auto& size = change_data["size"];
                    obj->set_size(size["width"], size["height"]);
                }
                break;
                
            default:
                // Unsupported event type for object
                throw std::runtime_error("Unsupported event type for object: " + 
                                        mcp::state::StateEvent::event_type_to_string(event_type));
        }
    } catch (const std::exception& e) {
        error(std::string("Failed to process object change: ") + e.what());
    }
}

void mcp_state_sync::process_parameter_change(const mcp::state::StateEvent::EventType& event_type,
                                            const std::string& param_id, const json& change_data) {
    try {
        // Parameter changes need to know which patch and object they belong to
        if (!change_data.contains("patchId") || !change_data.contains("objectId")) {
            throw std::runtime_error("Missing required fields 'patchId' or 'objectId'");
        }
        
        std::string patch_id = change_data["patchId"];
        std::string object_id = change_data["objectId"];
        
        if (!current_session->has_patch(patch_id)) {
            throw std::runtime_error("Patch not found: " + patch_id);
        }
        
        auto patch = current_session->get_patch(patch_id);
        
        if (!patch->has_object(object_id)) {
            throw std::runtime_error("Object not found: " + object_id);
        }
        
        auto object = patch->get_object(object_id);
        
        switch (event_type) {
            case mcp::state::StateEvent::EventType::ParamChanged:
                // Update parameter value
                if (!change_data.contains("name") || !change_data.contains("value")) {
                    throw std::runtime_error("Missing required fields 'name' or 'value'");
                }
                
                {
                    std::string param_name = change_data["name"];
                    json param_value = change_data["value"];
                    
                    // Add parameter if it doesn't exist
                    if (!object->has_parameter(param_name)) {
                        std::string param_type = change_data.value("type", "any");
                        bool is_read_only = change_data.value("isReadOnly", false);
                        object->add_parameter(mcp::state::Parameter(param_name, param_value, param_type, is_read_only));
                    } else {
                        // Update existing parameter
                        object->update_parameter(param_name, param_value);
                    }
                    
                    if (debug_mode) {
                        cout << "mcp.state_sync: updated parameter " << param_name << " for object " << object_id << endl;
                    }
                }
                break;
                
            default:
                // Unsupported event type for parameter
                throw std::runtime_error("Unsupported event type for parameter: " + 
                                        mcp::state::StateEvent::event_type_to_string(event_type));
        }
    } catch (const std::exception& e) {
        error(std::string("Failed to process parameter change: ") + e.what());
    }
}

void mcp_state_sync::process_connection_change(const mcp::state::StateEvent::EventType& event_type,
                                             const std::string& connection_id, const json& change_data) {
    try {
        // Connection changes need to know which patch they belong to
        if (!change_data.contains("patchId")) {
            throw std::runtime_error("Missing required field 'patchId'");
        }
        
        std::string patch_id = change_data["patchId"];
        
        if (!current_session->has_patch(patch_id)) {
            throw std::runtime_error("Patch not found: " + patch_id);
        }
        
        auto patch = current_session->get_patch(patch_id);
        
        switch (event_type) {
            case mcp::state::StateEvent::EventType::Connected:
                // Create new connection
                if (!change_data.contains("sourceId") || 
                    !change_data.contains("sourceOutlet") || 
                    !change_data.contains("destinationId") || 
                    !change_data.contains("destinationInlet")) {
                    throw std::runtime_error("Missing required connection fields");
                }
                
                {
                    std::string source_id = change_data["sourceId"];
                    int source_outlet = change_data["sourceOutlet"];
                    std::string dest_id = change_data["destinationId"];
                    int dest_inlet = change_data["destinationInlet"];
                    
                    auto conn = std::make_shared<mcp::state::Connection>(
                        connection_id, source_id, source_outlet, dest_id, dest_inlet);
                    
                    patch->add_connection(conn);
                    
                    if (debug_mode) {
                        cout << "mcp.state_sync: created connection from " << source_id 
                             << " outlet " << source_outlet << " to " << dest_id 
                             << " inlet " << dest_inlet << endl;
                    }
                }
                break;
                
            case mcp::state::StateEvent::EventType::Disconnected:
                // Delete connection
                if (patch->has_connection(connection_id)) {
                    patch->remove_connection(connection_id);
                    
                    if (debug_mode) {
                        cout << "mcp.state_sync: deleted connection " << connection_id << endl;
                    }
                }
                break;
                
            default:
                // Unsupported event type for connection
                throw std::runtime_error("Unsupported event type for connection: " + 
                                        mcp::state::StateEvent::event_type_to_string(event_type));
        }
    } catch (const std::exception& e) {
        error(std::string("Failed to process connection change: ") + e.what());
    }
}

void mcp_state_sync::process_global_setting_change(const mcp::state::StateEvent::EventType& event_type,
                                                 const std::string& setting_id, const json& change_data) {
    try {
        switch (event_type) {
            case mcp::state::StateEvent::EventType::Updated:
                // Update global setting
                if (!change_data.contains("value")) {
                    throw std::runtime_error("Missing required field 'value'");
                }
                
                current_session->get_global_settings().set_setting(setting_id, change_data["value"]);
                
                if (debug_mode) {
                    cout << "mcp.state_sync: updated global setting " << setting_id << endl;
                }
                break;
                
            default:
                // Unsupported event type for global setting
                throw std::runtime_error("Unsupported event type for global setting: " + 
                                        mcp::state::StateEvent::event_type_to_string(event_type));
        }
    } catch (const std::exception& e) {
        error(std::string("Failed to process global setting change: ") + e.what());
    }
}

// Differential synchronization implementation
void mcp_state_sync::handle_diff_sync(const symbol& request_id, long long last_sync_timestamp) {
    if (!is_initialized || !current_session) {
        error("Cannot handle diff sync request: State engine not initialized");
        return;
    }
    
    if (debug_mode) {
        cout << "mcp.state_sync: handling differential sync for request " << request_id
             << " with timestamp " << last_sync_timestamp << endl;
    }
    
    try {
        std::lock_guard<std::mutex> lock(state_mutex);
        
        // Create response dictionary
        c74::min::dict response;
        response["requestId"] = request_id;
        
        // Get current timestamp for response
        auto current_timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        // If the last sync timestamp is very old or 0, just send full state
        if (last_sync_timestamp == 0 || 
            (current_timestamp - last_sync_timestamp) > (24 * 60 * 60 * 1000000000LL)) { // 24 hours in nanoseconds
            
            if (debug_mode) {
                cout << "mcp.state_sync: timestamp too old, sending full state snapshot" << endl;
            }
            
            // For very old timestamps, send a full snapshot instead
            json full_state = current_session->to_json();
            response["type"] = "full_snapshot";
            response["data"] = std::string(full_state.dump());
            response["timestamp"] = current_timestamp;
            response_outlet.send("sync_response", response);
            return;
        }
        
        // Find all state changes since the last sync timestamp
        json changes = json::array();
        
        // Gather all new or modified objects, connections, etc.
        for (const auto& patch_pair : current_session->get_patches()) {
            auto patch = patch_pair.second;
            
            // Check if patch was modified after last sync
            if (patch->get_last_modified_time() > last_sync_timestamp) {
                json patch_change = {
                    {"category", "patch"},
                    {"id", patch->get_id()},
                    {"lastModified", patch->get_last_modified_time()},
                    {"data", patch->to_json()}
                };
                changes.push_back(patch_change);
                
                // Check objects in this patch
                for (const auto& obj_pair : patch->get_objects()) {
                    auto obj = obj_pair.second;
                    if (obj->get_last_modified_time() > last_sync_timestamp) {
                        json obj_change = {
                            {"category", "object"},
                            {"id", obj->get_id()},
                            {"patchId", patch->get_id()},
                            {"lastModified", obj->get_last_modified_time()},
                            {"data", obj->to_json()}
                        };
                        changes.push_back(obj_change);
                    }
                }
                
                // Check connections in this patch
                for (const auto& conn_pair : patch->get_connections()) {
                    auto conn = conn_pair.second;
                    if (conn->get_last_modified_time() > last_sync_timestamp) {
                        json conn_change = {
                            {"category", "connection"},
                            {"id", conn->get_id()},
                            {"patchId", patch->get_id()},
                            {"lastModified", conn->get_last_modified_time()},
                            {"data", conn->to_json()}
                        };
                        changes.push_back(conn_change);
                    }
                }
            }
        }
        
        // Check global settings
        if (current_session->get_global_settings().get_last_modified_time() > last_sync_timestamp) {
            json settings_change = {
                {"category", "global_setting"},
                {"id", "all"},
                {"lastModified", current_session->get_global_settings().get_last_modified_time()},
                {"data", current_session->get_global_settings().to_json()}
            };
            changes.push_back(settings_change);
        }
        
        // Also include information about deleted items since last sync
        // This would require maintaining a deletion log - for now using a simplified approach
        // TODO: Implement proper deletion tracking
        
        response["type"] = "differential";
        response["baseTimestamp"] = last_sync_timestamp;
        response["currentTimestamp"] = current_timestamp;
        response["changeCount"] = changes.size();
        response["data"] = std::string(changes.dump());
        
        // Send response
        response_outlet.send("sync_response", response);
        
        if (debug_mode) {
            cout << "mcp.state_sync: sent differential sync with " << changes.size() 
                 << " changes for request " << request_id << endl;
        }
        
    } catch (const std::exception& e) {
        error(std::string("Failed to handle differential sync request: ") + e.what());
        
        // Send error response
        c74::min::dict error_response;
        error_response["requestId"] = request_id;
        error_response["error"] = e.what();
        error_outlet.send("sync_error", error_response);
    }
}

// Compute differences between state snapshots
std::vector<mcp::state::StateDiff> mcp_state_sync::compute_state_diff(const json& base_state, 
                                                                   const json& current_state) {
    std::vector<mcp::state::StateDiff> diff_list;
    
    try {
        // Recursively compare JSON structures
        std::function<void(const json&, const json&, const std::string&)> compare_json = 
            [&](const json& base, const json& current, const std::string& path) {
                // Handle different types
                if (base.type() != current.type()) {
                    diff_list.emplace_back(mcp::state::StateDiff::Operation::Replace, path, current);
                    return;
                }
                
                // Handle based on type
                if (current.is_object()) {
                    // Compare object properties
                    for (auto it = current.begin(); it != current.end(); ++it) {
                        std::string key = it.key();
                        std::string new_path = path.empty() ? key : path + "/" + key;
                        
                        if (base.contains(key)) {
                            // Recursive compare for existing keys
                            compare_json(base[key], it.value(), new_path);
                        } else {
                            // New key added
                            diff_list.emplace_back(mcp::state::StateDiff::Operation::Add, new_path, it.value());
                        }
                    }
                    
                    // Check for removed keys
                    for (auto it = base.begin(); it != base.end(); ++it) {
                        std::string key = it.key();
                        if (!current.contains(key)) {
                            std::string new_path = path.empty() ? key : path + "/" + key;
                            diff_list.emplace_back(mcp::state::StateDiff::Operation::Remove, new_path);
                        }
                    }
                } else if (current.is_array()) {
                    // For arrays, we'll use a simplified approach that works for most cases
                    // In a real implementation, you might want a more sophisticated array diffing algorithm
                    if (base != current) {
                        diff_list.emplace_back(mcp::state::StateDiff::Operation::Replace, path, current);
                    }
                } else {
                    // For primitive types (string, number, boolean, null)
                    if (base != current) {
                        diff_list.emplace_back(mcp::state::StateDiff::Operation::Replace, path, current);
                    }
                }
            };
            
        // Start the recursive comparison
        compare_json(base_state, current_state, "");
        
    } catch (const std::exception& e) {
        // In case of any errors, return an empty diff list
        diff_list.clear();
        error(std::string("Error computing state diff: ") + e.what());
    }
    
    return diff_list;
}

// Save current state to a file
void mcp_state_sync::save_current_state(const symbol& request_id, const symbol& path) {
    if (!is_initialized || !current_session) {
        error("Cannot save state: State engine not initialized");
        return;
    }
    
    try {
        std::lock_guard<std::mutex> lock(state_mutex);
        
        // Expand path if it starts with ~
        std::string filepath = path.c_str();
        if (!filepath.empty() && filepath[0] == '~') {
            // Get home directory
            const char* home = getenv("HOME");
            if (home) {
                filepath = std::string(home) + filepath.substr(1);
            } else {
                throw std::runtime_error("Could not expand home directory");
            }
        }
        
        // Create directory if it doesn't exist
        fs::path file_path(filepath);
        fs::create_directories(file_path.parent_path());
        
        // Serialize current session state
        json state_json = current_session->to_json();
        
        // Add metadata
        state_json["__metadata"] = {
            {"version", "1.0"},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"format", "mcp_state"}
        };
        
        // Save to file
        std::ofstream file(filepath);
        if (!file) {
            throw std::runtime_error("Failed to open file for writing: " + filepath);
        }
        
        file << state_json.dump(2); // Pretty print with 2-space indent
        file.close();
        
        if (debug_mode) {
            cout << "mcp.state_sync: state saved to " << filepath << endl;
        }
        
        // Create response dictionary
        c74::min::dict response;
        response["requestId"] = request_id;
        response["path"] = filepath;
        response["success"] = true;
        response["timestamp"] = state_json["__metadata"]["timestamp"];
        
        // Send response
        response_outlet.send("save_response", response);
        
    } catch (const std::exception& e) {
        error(std::string("Failed to save state: ") + e.what());
        
        // Send error response
        c74::min::dict error_response;
        error_response["requestId"] = request_id;
        error_response["path"] = path;
        error_response["success"] = false;
        error_response["error"] = e.what();
        error_outlet.send("save_error", error_response);
    }
}

// Load state from a file
void mcp_state_sync::load_state_from_file(const symbol& request_id, const symbol& path) {
    try {
        // Expand path if it starts with ~
        std::string filepath = path.c_str();
        if (!filepath.empty() && filepath[0] == '~') {
            // Get home directory
            const char* home = getenv("HOME");
            if (home) {
                filepath = std::string(home) + filepath.substr(1);
            } else {
                throw std::runtime_error("Could not expand home directory");
            }
        }
        
        // Check if file exists
        if (!fs::exists(filepath)) {
            throw std::runtime_error("File does not exist: " + filepath);
        }
        
        // Read file
        std::ifstream file(filepath);
        if (!file) {
            throw std::runtime_error("Failed to open file for reading: " + filepath);
        }
        
        // Parse JSON
        json state_json = json::parse(file);
        
        // Verify metadata
        if (!state_json.contains("__metadata") || 
            !state_json["__metadata"].contains("format") || 
            state_json["__metadata"]["format"] != "mcp_state") {
            throw std::runtime_error("Invalid state file format");
        }
        
        // Create a new session from the loaded state
        auto new_session = std::make_unique<mcp::state::Session>(
            mcp::state::Session::from_json(state_json)
        );
        
        // Apply the loaded state (with synchronization)
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            current_session = std::move(new_session);
            
            // Send a full state notification
            send_state_snapshot();
        }
        
        if (debug_mode) {
            cout << "mcp.state_sync: state loaded from " << filepath << endl;
        }
        
        // Create response dictionary
        c74::min::dict response;
        response["requestId"] = request_id;
        response["path"] = filepath;
        response["success"] = true;
        response["sessionId"] = current_session->get_id();
        
        // Send response
        response_outlet.send("load_response", response);
        
    } catch (const std::exception& e) {
        error(std::string("Failed to load state: ") + e.what());
        
        // Send error response
        c74::min::dict error_response;
        error_response["requestId"] = request_id;
        error_response["path"] = path;
        error_response["success"] = false;
        error_response["error"] = e.what();
        error_outlet.send("load_error", error_response);
    }
}

// Conflict resolution implementation
json mcp_state_sync::resolve_conflicts(const json& local_state, const json& remote_state) {
    // Implementation depends on the chosen sync strategy
    std::string strategy_str = sync_strategy.c_str();
    
    if (strategy_str == "timestamp") {
        // Timestamp strategy: latest change wins
        return resolve_conflicts_by_timestamp(local_state, remote_state);
    } else if (strategy_str == "priority") {
        // Priority strategy: higher priority wins
        return resolve_conflicts_by_priority(local_state, remote_state);
    } else {
        // Default: use local state
        error("Unknown conflict resolution strategy, using local state");
        return local_state;
    }
}

// Helper function for timestamp-based conflict resolution
json mcp_state_sync::resolve_conflicts_by_timestamp(const json& local_state, const json& remote_state) {
    // Start with a copy of local state
    json resolved = local_state;
    
    try {
        // Compare timestamps for each component and select newest
        if (local_state.contains("lastModifiedTime") && remote_state.contains("lastModifiedTime")) {
            // Check high-level modification time
            if (remote_state["lastModifiedTime"] > local_state["lastModifiedTime"]) {
                return remote_state;  // Remote is newer, use it entirely
            }
        }
        
        // For patches, check each patch individually
        if (local_state.contains("patches") && remote_state.contains("patches")) {
            for (const auto& remote_patch : remote_state["patches"]) {
                std::string patch_id = remote_patch["id"];
                bool found = false;
                
                // Find corresponding patch in local state
                for (auto& local_patch : resolved["patches"]) {
                    if (local_patch["id"] == patch_id) {
                        // Compare timestamps
                        if (remote_patch["lastModifiedTime"] > local_patch["lastModifiedTime"]) {
                            // Remote patch is newer, replace local
                            local_patch = remote_patch;
                        }
                        found = true;
                        break;
                    }
                }
                
                // If not found in local, add it
                if (!found) {
                    resolved["patches"].push_back(remote_patch);
                }
            }
        }
        
        // Similar handling for global settings
        if (local_state.contains("globalSettings") && remote_state.contains("globalSettings")) {
            if (remote_state["globalSettings"].contains("lastModifiedTime") && 
                local_state["globalSettings"].contains("lastModifiedTime")) {
                
                if (remote_state["globalSettings"]["lastModifiedTime"] > 
                    local_state["globalSettings"]["lastModifiedTime"]) {
                    resolved["globalSettings"] = remote_state["globalSettings"];
                }
            }
        }
        
    } catch (const std::exception& e) {
        error(std::string("Error in timestamp conflict resolution: ") + e.what());
        // On error, fall back to local state
        return local_state;
    }
    
    return resolved;
}

// Helper function for priority-based conflict resolution
json mcp_state_sync::resolve_conflicts_by_priority(const json& local_state, const json& remote_state) {
    // Start with a copy of local state
    json resolved = local_state;
    
    try {
        // Priority strategy: predefined priorities for different components
        // Implement a more sophisticated priority scheme if needed
        
        // Check for presence of priority indicators
        if (remote_state.contains("priority") && local_state.contains("priority")) {
            if (remote_state["priority"] > local_state["priority"]) {
                return remote_state;  // Remote has higher priority
            }
        }
        
        // Individual patch priority handling
        if (local_state.contains("patches") && remote_state.contains("patches")) {
            for (const auto& remote_patch : remote_state["patches"]) {
                std::string patch_id = remote_patch["id"];
                bool found = false;
                
                // Find corresponding patch in local state
                for (auto& local_patch : resolved["patches"]) {
                    if (local_patch["id"] == patch_id) {
                        // Check priority if available
                        if (remote_patch.contains("priority") && local_patch.contains("priority")) {
                            if (remote_patch["priority"] > local_patch["priority"]) {
                                // Remote patch has higher priority
                                local_patch = remote_patch;
                            }
                        }
                        found = true;
                        break;
                    }
                }
                
                // If not found in local, add it
                if (!found) {
                    resolved["patches"].push_back(remote_patch);
                }
            }
        }
        
    } catch (const std::exception& e) {
        error(std::string("Error in priority conflict resolution: ") + e.what());
        // On error, fall back to local state
        return local_state;
    }
    
    return resolved;
}

MIN_EXTERNAL(mcp_state_sync);
