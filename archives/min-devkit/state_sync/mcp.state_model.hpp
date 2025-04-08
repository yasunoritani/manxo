#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace mcp {
namespace state {

/**
 * @brief Parameter of a Max object
 * 
 * Represents a parameter/attribute of a Max object with its name, value, and metadata.
 */
class Parameter {
public:
    Parameter(const std::string& name, const json& value, const std::string& type = "any", bool is_read_only = false);
    
    const std::string& get_name() const { return name; }
    const json& get_value() const { return value; }
    const std::string& get_type() const { return type; }
    bool is_read_only() const { return read_only; }
    
    void set_value(const json& new_value);
    json to_json() const;
    static Parameter from_json(const json& j);
    
private:
    std::string name;
    json value;
    std::string type;
    bool read_only;
};

/**
 * @brief Max object in a patch
 * 
 * Represents a Max object with its type, position, parameters, inlets, and outlets.
 */
class MaxObject {
public:
    MaxObject(const std::string& id, const std::string& type);
    
    const std::string& get_id() const { return id; }
    const std::string& get_type() const { return type; }
    const std::pair<int, int>& get_position() const { return position; }
    const std::pair<int, int>& get_size() const { return size; }
    
    void set_position(int x, int y);
    void set_size(int width, int height);
    
    void add_parameter(const Parameter& param);
    void update_parameter(const std::string& name, const json& value);
    bool has_parameter(const std::string& name) const;
    const Parameter& get_parameter(const std::string& name) const;
    
    void set_inlets(int count);
    void set_outlets(int count);
    int get_inlet_count() const { return inlets; }
    int get_outlet_count() const { return outlets; }
    
    json to_json() const;
    static MaxObject from_json(const json& j);
    
private:
    std::string id;
    std::string type;
    std::pair<int, int> position{0, 0};
    std::pair<int, int> size{0, 0};
    std::unordered_map<std::string, Parameter> parameters;
    int inlets{0};
    int outlets{0};
    mutable std::mutex object_mutex;
};

/**
 * @brief Connection between Max objects
 * 
 * Represents a connection between outlets and inlets of Max objects.
 */
class Connection {
public:
    Connection(const std::string& id, 
               const std::string& source_id, int source_outlet,
               const std::string& dest_id, int dest_inlet);
    
    const std::string& get_id() const { return id; }
    const std::string& get_source_id() const { return source_id; }
    int get_source_outlet() const { return source_outlet; }
    const std::string& get_destination_id() const { return destination_id; }
    int get_destination_inlet() const { return destination_inlet; }
    
    json to_json() const;
    static Connection from_json(const json& j);
    
private:
    std::string id;
    std::string source_id;
    int source_outlet;
    std::string destination_id;
    int destination_inlet;
};

/**
 * @brief Max patch
 * 
 * Represents a Max patch with its objects and connections.
 */
class Patch {
public:
    Patch(const std::string& id, const std::string& name, const std::string& path = "");
    
    const std::string& get_id() const { return id; }
    const std::string& get_name() const { return name; }
    const std::string& get_path() const { return path; }
    bool is_modified() const { return modified; }
    
    void set_name(const std::string& new_name);
    void set_path(const std::string& new_path);
    void set_modified(bool is_modified);
    
    void add_object(std::shared_ptr<MaxObject> object);
    void remove_object(const std::string& object_id);
    bool has_object(const std::string& object_id) const;
    std::shared_ptr<MaxObject> get_object(const std::string& object_id) const;
    
    void add_connection(std::shared_ptr<Connection> connection);
    void remove_connection(const std::string& connection_id);
    bool has_connection(const std::string& connection_id) const;
    std::shared_ptr<Connection> get_connection(const std::string& connection_id) const;
    
    json to_json() const;
    static Patch from_json(const json& j);
    
private:
    std::string id;
    std::string name;
    std::string path;
    bool modified{false};
    std::unordered_map<std::string, std::shared_ptr<MaxObject>> objects;
    std::unordered_map<std::string, std::shared_ptr<Connection>> connections;
    mutable std::mutex patch_mutex;
};

/**
 * @brief Global settings
 * 
 * Represents global settings for the Max session.
 */
class GlobalSettings {
public:
    GlobalSettings() = default;
    
    void set_setting(const std::string& name, const json& value);
    const json& get_setting(const std::string& name) const;
    bool has_setting(const std::string& name) const;
    
    json to_json() const;
    static GlobalSettings from_json(const json& j);
    
private:
    std::unordered_map<std::string, json> settings;
    mutable std::mutex settings_mutex;
};

/**
 * @brief Max session
 * 
 * Represents a Max session with its patches and global settings.
 */
class Session {
public:
    Session(const std::string& id, const std::string& name);
    
    const std::string& get_id() const { return id; }
    const std::string& get_name() const { return name; }
    long long get_start_time() const { return start_time; }
    
    const std::unordered_map<std::string, std::shared_ptr<Patch>>& get_patches() const { return patches; }
    
    void set_name(const std::string& new_name);
    
    void add_patch(std::shared_ptr<Patch> patch);
    void remove_patch(const std::string& patch_id);
    bool has_patch(const std::string& patch_id) const;
    std::shared_ptr<Patch> get_patch(const std::string& patch_id) const;
    
    GlobalSettings& get_global_settings() { return global_settings; }
    const GlobalSettings& get_global_settings() const { return global_settings; }
    
    json to_json() const;
    static Session from_json(const json& j);
    
private:
    std::string id;
    std::string name;
    long long start_time;
    std::unordered_map<std::string, std::shared_ptr<Patch>> patches;
    GlobalSettings global_settings;
    mutable std::mutex session_mutex;
};

/**
 * @brief State event
 * 
 * Represents a state change event for history tracking.
 */
class StateEvent {
public:
    enum class Category {
        Session,
        Patch,
        Object,
        Parameter,
        Connection,
        GlobalSetting
    };
    
    enum class EventType {
        Created,
        Updated,
        Deleted,
        Connected,
        Disconnected,
        Moved,
        Resized,
        ParamChanged,
        StateChanged
    };
    
    StateEvent(Category category, EventType type, const std::string& object_id, 
              const json& data, long long timestamp = 0);
    
    Category get_category() const { return category; }
    EventType get_event_type() const { return event_type; }
    const std::string& get_object_id() const { return object_id; }
    const json& get_data() const { return data; }
    long long get_timestamp() const { return timestamp; }
    
    json to_json() const;
    static StateEvent from_json(const json& j);
    
    static std::string category_to_string(Category category);
    static std::string event_type_to_string(EventType type);
    static Category string_to_category(const std::string& str);
    static EventType string_to_event_type(const std::string& str);
    
private:
    Category category;
    EventType event_type;
    std::string object_id;
    json data;
    long long timestamp;
};

/**
 * @brief State change
 * 
 * Wrapper for state change events with notification utilities.
 */
class StateChange {
public:
    StateChange(const StateEvent& event);
    StateChange(StateEvent::Category category, StateEvent::EventType type, 
               const std::string& object_id, const json& data);
    
    const StateEvent& get_event() const { return event; }
    
    json to_notification_json() const;
    std::string to_osc_address() const;
    
private:
    StateEvent event;
};

/**
 * @brief State difference
 * 
 * Represents a difference in state for differential synchronization.
 */
class StateDiff {
public:
    enum class Operation {
        Add,
        Replace,
        Remove,
        Move
    };
    
    StateDiff(Operation op, const std::string& path, const json& value = json());
    
    Operation get_operation() const { return operation; }
    const std::string& get_path() const { return path; }
    const json& get_value() const { return value; }
    
    json to_json() const;
    static StateDiff from_json(const json& j);
    
    static std::string operation_to_string(Operation op);
    static Operation string_to_operation(const std::string& str);
    
private:
    Operation operation;
    std::string path;
    json value;
};

} // namespace state
} // namespace mcp
