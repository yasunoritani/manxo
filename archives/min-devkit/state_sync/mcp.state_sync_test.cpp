/// @file
/// @ingroup     mcpmodules
/// @copyright   Copyright 2025 The Max 9-Claude Desktop Integration Project. All rights reserved.
/// @license     Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min_unittest.h"
#include "mcp.state_model.hpp"
#include <chrono>
#include <thread>
#include <memory>
#include <vector>

using namespace c74::min;
using namespace c74::min::test;
using namespace mcp::state;

SCENARIO("State model basic functionality") {
    GIVEN("A basic state model hierarchy") {
        // Create a session
        auto session = std::make_shared<Session>("session-1", "Test Session");
        
        // Add a patch to the session
        auto patch = std::make_shared<Patch>("patch-1", "Test Patch", "~/Documents/Max 9/patches/test.maxpat");
        session->add_patch(patch);
        
        // Add objects to the patch
        auto obj1 = std::make_shared<MaxObject>("obj-1", "cycle~");
        obj1->set_position(100, 150);
        obj1->set_size(64, 64);
        obj1->set_inlets(1);
        obj1->set_outlets(1);
        obj1->add_parameter(Parameter("frequency", 440.0, "float", false));
        
        auto obj2 = std::make_shared<MaxObject>("obj-2", "gain~");
        obj2->set_position(200, 150);
        obj2->set_size(64, 64);
        obj2->set_inlets(1);
        obj2->set_outlets(1);
        obj2->add_parameter(Parameter("level", 0.5, "float", false));
        
        patch->add_object(obj1);
        patch->add_object(obj2);
        
        // Create a connection between the objects
        auto conn = std::make_shared<Connection>("conn-1", 
                                               "obj-1", 0, 
                                               "obj-2", 0);
        patch->add_connection(conn);
        
        // Set global settings
        session->get_global_settings().set_setting("oscPort", 7400);
        session->get_global_settings().set_setting("sampling_rate", 44100);
        
        WHEN("Converting to JSON") {
            json j = session->to_json();
            
            THEN("JSON representation is correct") {
                REQUIRE(j["id"] == "session-1");
                REQUIRE(j["name"] == "Test Session");
                REQUIRE(j["patches"].size() == 1);
                REQUIRE(j["patches"][0]["id"] == "patch-1");
                REQUIRE(j["patches"][0]["objects"].size() == 2);
                REQUIRE(j["patches"][0]["connections"].size() == 1);
                REQUIRE(j["globalSettings"]["oscPort"] == 7400);
            }
            
            AND_WHEN("Converting back from JSON") {
                auto restored_session = Session::from_json(j);
                
                THEN("The restored session matches the original") {
                    REQUIRE(restored_session.get_id() == session->get_id());
                    REQUIRE(restored_session.get_name() == session->get_name());
                    REQUIRE(restored_session.has_patch("patch-1"));
                    
                    auto restored_patch = restored_session.get_patch("patch-1");
                    REQUIRE(restored_patch->get_name() == patch->get_name());
                    REQUIRE(restored_patch->has_object("obj-1"));
                    REQUIRE(restored_patch->has_object("obj-2"));
                    REQUIRE(restored_patch->has_connection("conn-1"));
                    
                    auto restored_obj1 = restored_patch->get_object("obj-1");
                    REQUIRE(restored_obj1->get_type() == "cycle~");
                    REQUIRE(restored_obj1->has_parameter("frequency"));
                    REQUIRE(restored_obj1->get_parameter("frequency").get_value() == 440.0);
                    
                    // Check global settings
                    REQUIRE(restored_session.get_global_settings().has_setting("oscPort"));
                    REQUIRE(restored_session.get_global_settings().get_setting("oscPort") == 7400);
                }
            }
        }
    }
}

SCENARIO("State events and changes") {
    GIVEN("State events for different operations") {
        // Create state events
        StateEvent created_event(
            StateEvent::Category::Object, 
            StateEvent::EventType::Created,
            "obj-1",
            json{{"type", "slider"}, {"position", {{"x", 100}, {"y", 150}}}}
        );
        
        StateEvent updated_event(
            StateEvent::Category::Parameter,
            StateEvent::EventType::ParamChanged,
            "obj-1",
            json{{"name", "value"}, {"value", 0.75}}
        );
        
        StateEvent deleted_event(
            StateEvent::Category::Object,
            StateEvent::EventType::Deleted,
            "obj-1",
            json::object()
        );
        
        WHEN("Converting events to JSON") {
            json j1 = created_event.to_json();
            json j2 = updated_event.to_json();
            json j3 = deleted_event.to_json();
            
            THEN("JSON representations are correct") {
                REQUIRE(j1["category"] == "object");
                REQUIRE(j1["eventType"] == "created");
                REQUIRE(j1["objectId"] == "obj-1");
                
                REQUIRE(j2["category"] == "parameter");
                REQUIRE(j2["eventType"] == "paramChanged");
                
                REQUIRE(j3["category"] == "object");
                REQUIRE(j3["eventType"] == "deleted");
            }
            
            AND_WHEN("Creating state changes from events") {
                StateChange created_change(created_event);
                StateChange updated_change(updated_event);
                StateChange deleted_change(deleted_event);
                
                THEN("OSC addresses are correctly formed") {
                    REQUIRE(created_change.to_osc_address() == "/max/state/object/created");
                    REQUIRE(updated_change.to_osc_address() == "/max/state/parameter/paramChanged");
                    REQUIRE(deleted_change.to_osc_address() == "/max/state/object/deleted");
                }
                
                AND_THEN("Notification JSON is correctly formed") {
                    json n1 = created_change.to_notification_json();
                    REQUIRE(n1["category"] == "object");
                    REQUIRE(n1["eventType"] == "created");
                    REQUIRE(n1["objectId"] == "obj-1");
                    REQUIRE(n1["data"]["type"] == "slider");
                }
            }
        }
    }
}

SCENARIO("State differences for differential sync") {
    GIVEN("State differences for various operations") {
        // Create state diffs
        StateDiff add_diff(
            StateDiff::Operation::Add,
            "/patches/0/objects/-",
            json{{"id", "obj-3"}, {"type", "number"}}
        );
        
        StateDiff replace_diff(
            StateDiff::Operation::Replace,
            "/patches/0/objects/1/parameters/level/value",
            0.75
        );
        
        StateDiff remove_diff(
            StateDiff::Operation::Remove,
            "/patches/0/connections/0"
        );
        
        StateDiff move_diff(
            StateDiff::Operation::Move,
            "/patches/0/objects/1",
            json{{"from", 1}, {"to", 2}}
        );
        
        WHEN("Converting diffs to JSON") {
            json j1 = add_diff.to_json();
            json j2 = replace_diff.to_json();
            json j3 = remove_diff.to_json();
            json j4 = move_diff.to_json();
            
            THEN("JSON representations are correct") {
                REQUIRE(j1["op"] == "add");
                REQUIRE(j1["path"] == "/patches/0/objects/-");
                REQUIRE(j1["value"]["id"] == "obj-3");
                
                REQUIRE(j2["op"] == "replace");
                REQUIRE(j2["path"] == "/patches/0/objects/1/parameters/level/value");
                REQUIRE(j2["value"] == 0.75);
                
                REQUIRE(j3["op"] == "remove");
                REQUIRE(j3["path"] == "/patches/0/connections/0");
                
                REQUIRE(j4["op"] == "move");
                REQUIRE(j4["path"] == "/patches/0/objects/1");
                REQUIRE(j4["value"]["from"] == 1);
                REQUIRE(j4["value"]["to"] == 2);
            }
            
            AND_WHEN("Creating StateDiff from JSON") {
                auto restored_add = StateDiff::from_json(j1);
                auto restored_replace = StateDiff::from_json(j2);
                auto restored_remove = StateDiff::from_json(j3);
                auto restored_move = StateDiff::from_json(j4);
                
                THEN("Restored diffs match original") {
                    REQUIRE(restored_add.get_operation() == StateDiff::Operation::Add);
                    REQUIRE(restored_add.get_path() == "/patches/0/objects/-");
                    REQUIRE(restored_add.get_value()["id"] == "obj-3");
                    
                    REQUIRE(restored_replace.get_operation() == StateDiff::Operation::Replace);
                    REQUIRE(restored_replace.get_path() == "/patches/0/objects/1/parameters/level/value");
                    REQUIRE(restored_replace.get_value() == 0.75);
                    
                    REQUIRE(restored_remove.get_operation() == StateDiff::Operation::Remove);
                    REQUIRE(restored_remove.get_path() == "/patches/0/connections/0");
                    
                    REQUIRE(restored_move.get_operation() == StateDiff::Operation::Move);
                    REQUIRE(restored_move.get_path() == "/patches/0/objects/1");
                }
                REQUIRE(j3.contains("value") == false);
                
                REQUIRE(j4["op"] == "move");
                REQUIRE(j4["path"] == "/patches/0/objects/1");
                REQUIRE(j4["value"]["from"] == 1);
                REQUIRE(j4["value"]["to"] == 2);
            }
            
            AND_WHEN("Converting back from JSON") {
                auto restored_add = StateDiff::from_json(j1);
                auto restored_replace = StateDiff::from_json(j2);
                auto restored_remove = StateDiff::from_json(j3);
                auto restored_move = StateDiff::from_json(j4);
                
                THEN("The restored diffs match the originals") {
                    REQUIRE(restored_add.get_operation() == StateDiff::Operation::Add);
                    REQUIRE(restored_add.get_path() == "/patches/0/objects/-");
                    REQUIRE(restored_add.get_value()["id"] == "obj-3");
                    
                    REQUIRE(restored_replace.get_operation() == StateDiff::Operation::Replace);
                    REQUIRE(restored_replace.get_path() == "/patches/0/objects/1/parameters/level/value");
                    REQUIRE(restored_replace.get_value() == 0.75);
                    
                    REQUIRE(restored_remove.get_operation() == StateDiff::Operation::Remove);
                    REQUIRE(restored_remove.get_path() == "/patches/0/connections/0");
                    
                    REQUIRE(restored_move.get_operation() == StateDiff::Operation::Move);
                    REQUIRE(restored_move.get_path() == "/patches/0/objects/1");
                    REQUIRE(restored_move.get_value()["from"] == 1);
                    REQUIRE(restored_move.get_value()["to"] == 2);
                }
            }
        }
    }
}

SCENARIO("State difference computation") {
    GIVEN("Two different state snapshots") {
        // Create base state
        json base_state = {
            {"id", "session-1"},
            {"name", "Test Session"},
            {"patches", json::array({
                {
                    {"id", "patch-1"},
                    {"name", "Test Patch"},
                    {"objects", json::array({
                        {
                            {"id", "obj-1"},
                            {"type", "slider"},
                            {"parameters", json::array({
                                {
                                    {"name", "value"},
                                    {"value", 0.5}
                                }
                            })}
                        },
                        {
                            {"id", "obj-2"},
                            {"type", "number"},
                            {"parameters", json::array({
                                {
                                    {"name", "value"},
                                    {"value", 100}
                                }
                            })}
                        }
                    })},
                    {"connections", json::array({
                        {
                            {"id", "conn-1"},
                            {"sourceId", "obj-1"},
                            {"sourceOutlet", 0},
                            {"destinationId", "obj-2"},
                            {"destinationInlet", 0}
                        }
                    })}
                }
            })},
            {"globalSettings", {
                {"oscPort", 7400}
            }}
        };
        
        // Create modified state with changes
        json modified_state = base_state;
        
        // 1. Modify an existing value
        modified_state["patches"][0]["objects"][0]["parameters"][0]["value"] = 0.75;
        
        // 2. Add a new object
        json new_object = {
            {"id", "obj-3"},
            {"type", "toggle"},
            {"parameters", json::array({
                {
                    {"name", "value"},
                    {"value", true}
                }
            })}
        };
        modified_state["patches"][0]["objects"].push_back(new_object);
        
        // 3. Remove a connection
        modified_state["patches"][0]["connections"] = json::array();
        
        // 4. Change a global setting
        modified_state["globalSettings"]["oscPort"] = 7500;
        
        // 5. Add a new global setting
        modified_state["globalSettings"]["autoConnect"] = true;
        
        // Create mock state sync engine object for testing
        class mock_state_sync {
        public:
            static std::vector<mcp::state::StateDiff> compute_state_diff(
                const json& base, const json& current) {
                std::vector<mcp::state::StateDiff> diff_list;
                
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
                compare_json(base, current, "");
                
                return diff_list;
            }
        };
        
        WHEN("Computing differences") {
            auto diffs = mock_state_sync::compute_state_diff(base_state, modified_state);
            
            THEN("Correct differences are identified") {
                // Count of specific operations
                int add_count = 0;
                int replace_count = 0;
                int remove_count = 0;
                
                // Track specific expected changes
                bool found_param_change = false;
                bool found_new_object = false;
                bool found_removed_connection = false;
                bool found_changed_setting = false;
                bool found_new_setting = false;
                
                for (const auto& diff : diffs) {
                    if (diff.get_operation() == StateDiff::Operation::Add) add_count++;
                    if (diff.get_operation() == StateDiff::Operation::Replace) replace_count++;
                    if (diff.get_operation() == StateDiff::Operation::Remove) remove_count++;
                    
                    // Check for specific changes
                    if (diff.get_operation() == StateDiff::Operation::Replace && 
                        diff.get_path().find("parameters/0/value") != std::string::npos && 
                        diff.get_value() == 0.75) {
                        found_param_change = true;
                    }
                    
                    if (diff.get_operation() == StateDiff::Operation::Add && 
                        diff.get_path().find("objects") != std::string::npos && 
                        diff.get_value()["id"] == "obj-3") {
                        found_new_object = true;
                    }
                    
                    if (diff.get_operation() == StateDiff::Operation::Replace && 
                        diff.get_path().find("connections") != std::string::npos) {
                        found_removed_connection = true;
                    }
                    
                    if (diff.get_operation() == StateDiff::Operation::Replace && 
                        diff.get_path() == "globalSettings/oscPort" && 
                        diff.get_value() == 7500) {
                        found_changed_setting = true;
                    }
                    
                    if (diff.get_operation() == StateDiff::Operation::Add && 
                        diff.get_path() == "globalSettings/autoConnect" && 
                        diff.get_value() == true) {
                        found_new_setting = true;
                    }
                }
                
                // Verify that all expected changes were found
                REQUIRE(found_param_change);
                REQUIRE(found_new_object);
                REQUIRE(found_removed_connection);
                REQUIRE(found_changed_setting);
                REQUIRE(found_new_setting);
            }
        }
    }
}

SCENARIO("Conflict resolution mechanisms") {
    GIVEN("Two conflicting state snapshots") {
        // Create local state
        json local_state = {
            {"id", "session-1"},
            {"name", "Local Session"},
            {"lastModifiedTime", 1000},  // Earlier timestamp
            {"patches", json::array({
                {
                    {"id", "patch-1"},
                    {"name", "Local Patch"},
                    {"lastModifiedTime", 900},
                    {"objects", json::array()}
                },
                {
                    {"id", "patch-2"},
                    {"name", "Shared Patch"},
                    {"lastModifiedTime", 1200},  // Local is newer
                    {"objects", json::array()}
                }
            })},
            {"globalSettings", {
                {"lastModifiedTime", 800},
                {"oscPort", 7400}
            }}
        };
        
        // Create remote state with conflicts
        json remote_state = {
            {"id", "session-1"},
            {"name", "Remote Session"},  // Conflict in session name
            {"lastModifiedTime", 1100},  // Later timestamp
            {"patches", json::array({
                {
                    {"id", "patch-1"},
                    {"name", "Remote Patch"},  // Conflict in patch name
                    {"lastModifiedTime", 1050},  // Remote is newer
                    {"objects", json::array()}
                },
                {
                    {"id", "patch-2"},
                    {"name", "Old Shared Patch"},  // Conflict in patch name
                    {"lastModifiedTime", 950},  // Local is newer
                    {"objects", json::array()}
                },
                {
                    {"id", "patch-3"},  // New patch in remote
                    {"name", "Remote Only Patch"},
                    {"lastModifiedTime", 1000},
                    {"objects", json::array()}
                }
            })},
            {"globalSettings", {
                {"lastModifiedTime", 1200},  // Remote is newer
                {"oscPort", 7500},  // Conflict in port number
                {"autoSync", true}  // New setting in remote
            }}
        };
        
        // Create mock state sync engine with conflict resolution methods
        class mock_state_sync {
        public:
            static json resolve_conflicts_by_timestamp(const json& local, const json& remote) {
                // Start with a copy of local state
                json resolved = local;
                
                // Compare timestamps for each component and select newest
                if (local.contains("lastModifiedTime") && remote.contains("lastModifiedTime")) {
                    // Check high-level modification time
                    if (remote["lastModifiedTime"] > local["lastModifiedTime"]) {
                        resolved["name"] = remote["name"];  // Use remote session name
                    }
                }
                
                // For patches, check each patch individually
                if (local.contains("patches") && remote.contains("patches")) {
                    // First, process existing patches in local state
                    for (auto& local_patch : resolved["patches"]) {
                        std::string patch_id = local_patch["id"];
                        
                        // Find corresponding patch in remote state
                        for (const auto& remote_patch : remote["patches"]) {
                            if (remote_patch["id"] == patch_id) {
                                // Compare timestamps
                                if (remote_patch["lastModifiedTime"] > local_patch["lastModifiedTime"]) {
                                    // Remote patch is newer, update name
                                    local_patch["name"] = remote_patch["name"];
                                }
                                break;
                            }
                        }
                    }
                    
                    // Then, add any patches that exist only in remote
                    for (const auto& remote_patch : remote["patches"]) {
                        std::string patch_id = remote_patch["id"];
                        bool found = false;
                        
                        for (const auto& local_patch : resolved["patches"]) {
                            if (local_patch["id"] == patch_id) {
                                found = true;
                                break;
                            }
                        }
                        
                        if (!found) {
                            resolved["patches"].push_back(remote_patch);
                        }
                    }
                }
                
                // Similar handling for global settings
                if (local.contains("globalSettings") && remote.contains("globalSettings")) {
                    if (remote["globalSettings"].contains("lastModifiedTime") && 
                        local["globalSettings"].contains("lastModifiedTime")) {
                        
                        if (remote["globalSettings"]["lastModifiedTime"] > 
                            local["globalSettings"]["lastModifiedTime"]) {
                            // Remote global settings are newer, use remote values
                            // but preserve local-only settings
                            for (auto it = remote["globalSettings"].begin(); it != remote["globalSettings"].end(); ++it) {
                                if (it.key() != "lastModifiedTime") {  // Skip the timestamp itself
                                    resolved["globalSettings"][it.key()] = it.value();
                                }
                            }
                        }
                    }
                }
                
                return resolved;
            }
            
            static json resolve_conflicts_by_priority(const json& local, const json& remote) {
                // Placeholder for priority-based resolution logic
                // In a real implementation, this would use priority metadata for resolution
                json resolved = local;
                
                // For global settings, remote has priority
                if (local.contains("globalSettings") && remote.contains("globalSettings")) {
                    for (auto it = remote["globalSettings"].begin(); it != remote["globalSettings"].end(); ++it) {
                        if (it.key() != "lastModifiedTime") {  // Skip timestamps
                            resolved["globalSettings"][it.key()] = it.value();
                        }
                    }
                }
                
                // For patches, use a mixed approach - keep local names but add remote-only patches
                if (local.contains("patches") && remote.contains("patches")) {
                    for (const auto& remote_patch : remote["patches"]) {
                        std::string patch_id = remote_patch["id"];
                        bool found = false;
                        
                        for (const auto& local_patch : resolved["patches"]) {
                            if (local_patch["id"] == patch_id) {
                                found = true;
                                break;
                            }
                        }
                        
                        if (!found) {
                            resolved["patches"].push_back(remote_patch);
                        }
                    }
                }
                
                return resolved;
            }
        };
        
        WHEN("Resolving conflicts using timestamp strategy") {
            auto resolved = mock_state_sync::resolve_conflicts_by_timestamp(local_state, remote_state);
            
            THEN("Resolution follows timestamp rules") {
                // Session level - remote is newer
                REQUIRE(resolved["name"] == "Remote Session");
                
                // Patches - patch specific timestamps
                bool found_remote_patch1 = false;
                bool found_local_patch2 = false;
                bool found_remote_patch3 = false;
                
                for (const auto& patch : resolved["patches"]) {
                    if (patch["id"] == "patch-1") {
                        // Remote patch-1 is newer
                        found_remote_patch1 = (patch["name"] == "Remote Patch");
                    }
                    if (patch["id"] == "patch-2") {
                        // Local patch-2 is newer
                        found_local_patch2 = (patch["name"] == "Shared Patch");
                    }
                    if (patch["id"] == "patch-3") {
                        // patch-3 only exists in remote
                        found_remote_patch3 = (patch["name"] == "Remote Only Patch");
                    }
                }
                
                REQUIRE(found_remote_patch1);
                REQUIRE(found_local_patch2);
                REQUIRE(found_remote_patch3);
                
                // Global settings - remote is newer
                REQUIRE(resolved["globalSettings"]["oscPort"] == 7500);
                REQUIRE(resolved["globalSettings"]["autoSync"] == true);
            }
        }
        
        WHEN("Resolving conflicts using priority strategy") {
            auto resolved = mock_state_sync::resolve_conflicts_by_priority(local_state, remote_state);
            
            THEN("Resolution follows priority rules") {
                // For our test implementation, remote has priority for global settings
                REQUIRE(resolved["globalSettings"]["oscPort"] == 7500);
                REQUIRE(resolved["globalSettings"]["autoSync"] == true);
                
                // But session name stays local per our simplified logic
                REQUIRE(resolved["name"] == "Local Session");
                
                // Remote-only patches should be added
                bool found_remote_patch3 = false;
                for (const auto& patch : resolved["patches"]) {
                    if (patch["id"] == "patch-3") {
                        found_remote_patch3 = true;
                        break;
                    }
                }
                REQUIRE(found_remote_patch3);
            }
        }
    }
}

SCENARIO("State persistence and loading") {
    GIVEN("A session with state to save and load") {
        // Create a test session
        auto session = std::make_shared<Session>("session-1", "Test Session");
        
        // Add a patch to the session
        auto patch = std::make_shared<Patch>("patch-1", "Test Patch", "~/Documents/Max 9/patches/test.maxpat");
        session->add_patch(patch);
        
        // Add objects to the patch
        auto obj1 = std::make_shared<MaxObject>("obj-1", "slider");
        obj1->add_parameter(Parameter("value", 0.75, "float", false));
        patch->add_object(obj1);
        
        // Set global settings
        session->get_global_settings().set_setting("oscPort", 7400);
        
        // Create a temporary file path for testing
        fs::path temp_dir = fs::temp_directory_path();
        fs::path test_file = temp_dir / "mcp_state_test.json";
        std::string test_path = test_file.string();
        
        // Mock state sync engine for save/load testing
        class mock_state_sync {
        public:
            static bool save_state_to_file(const Session& session, const std::string& path) {
                try {
                    // Serialize session state
                    json state_json = session.to_json();
                    
                    // Add metadata
                    state_json["__metadata"] = {
                        {"version", "1.0"},
                        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
                        {"format", "mcp_state"}
                    };
                    
                    // Save to file
                    std::ofstream file(path);
                    if (!file) {
                        return false;
                    }
                    
                    file << state_json.dump(2); // Pretty print with 2-space indent
                    file.close();
                    return true;
                } 
                catch (const std::exception&) {
                    return false;
                }
            }
            
            static std::unique_ptr<Session> load_state_from_file(const std::string& path) {
                try {
                    // Check if file exists
                    if (!fs::exists(path)) {
                        return nullptr;
                    }
                    
                    // Read file
                    std::ifstream file(path);
                    if (!file) {
                        return nullptr;
                    }
                    
                    // Parse JSON
                    json state_json = json::parse(file);
                    
                    // Verify metadata
                    if (!state_json.contains("__metadata") || 
                        !state_json["__metadata"].contains("format") || 
                        state_json["__metadata"]["format"] != "mcp_state") {
                        return nullptr;
                    }
                    
                    // Create a new session from the loaded state
                    return std::make_unique<Session>(Session::from_json(state_json));
                }
                catch (const std::exception&) {
                    return nullptr;
                }
            }
        };
        
        WHEN("Saving state to a file") {
            bool save_result = mock_state_sync::save_state_to_file(*session, test_path);
            
            THEN("Save operation succeeds") {
                REQUIRE(save_result);
                REQUIRE(fs::exists(test_path));
                
                AND_WHEN("Loading state from the file") {
                    auto loaded_session = mock_state_sync::load_state_from_file(test_path);
                    
                    THEN("Loaded session matches the original") {
                        REQUIRE(loaded_session != nullptr);
                        REQUIRE(loaded_session->get_id() == session->get_id());
                        REQUIRE(loaded_session->get_name() == session->get_name());
                        REQUIRE(loaded_session->has_patch("patch-1"));
                        
                        auto loaded_patch = loaded_session->get_patch("patch-1");
                        REQUIRE(loaded_patch->has_object("obj-1"));
                        
                        auto loaded_obj = loaded_patch->get_object("obj-1");
                        REQUIRE(loaded_obj->has_parameter("value"));
                        REQUIRE(loaded_obj->get_parameter("value").get_value() == 0.75);
                        
                        REQUIRE(loaded_session->get_global_settings().has_setting("oscPort"));
                        REQUIRE(loaded_session->get_global_settings().get_setting("oscPort") == 7400);
                    }
                }
            }
            
            // Clean up test file
            if (fs::exists(test_path)) {
                fs::remove(test_path);
            }
        }
    }
}
