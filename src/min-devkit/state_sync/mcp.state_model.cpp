#include "mcp.state_model.hpp"
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace mcp {
namespace state {

// Parameter implementation
Parameter::Parameter(const std::string& name, const json& value, const std::string& type, bool is_read_only)
    : name(name), value(value), type(type), read_only(is_read_only) {
}

void Parameter::set_value(const json& new_value) {
    if (read_only) {
        throw std::runtime_error("Cannot modify read-only parameter: " + name);
    }
    value = new_value;
}

json Parameter::to_json() const {
    json j;
    j["name"] = name;
    j["value"] = value;
    j["type"] = type;
    j["isReadOnly"] = read_only;
    return j;
}

Parameter Parameter::from_json(const json& j) {
    return Parameter(
        j["name"].get<std::string>(),
        j["value"],
        j["type"].get<std::string>(),
        j["isReadOnly"].get<bool>()
    );
}

// MaxObject implementation
MaxObject::MaxObject(const std::string& id, const std::string& type)
    : id(id), type(type) {
}

void MaxObject::set_position(int x, int y) {
    std::lock_guard<std::mutex> lock(object_mutex);
    position = {x, y};
}

void MaxObject::set_size(int width, int height) {
    std::lock_guard<std::mutex> lock(object_mutex);
    size = {width, height};
}

void MaxObject::add_parameter(const Parameter& param) {
    std::lock_guard<std::mutex> lock(object_mutex);
    parameters[param.get_name()] = param;
}

void MaxObject::update_parameter(const std::string& name, const json& value) {
    std::lock_guard<std::mutex> lock(object_mutex);
    if (parameters.find(name) == parameters.end()) {
        throw std::runtime_error("Parameter not found: " + name);
    }
    parameters[name].set_value(value);
}

bool MaxObject::has_parameter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(object_mutex);
    return parameters.find(name) != parameters.end();
}

const Parameter& MaxObject::get_parameter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(object_mutex);
    auto it = parameters.find(name);
    if (it == parameters.end()) {
        throw std::runtime_error("Parameter not found: " + name);
    }
    return it->second;
}

void MaxObject::set_inlets(int count) {
    inlets = count;
}

void MaxObject::set_outlets(int count) {
    outlets = count;
}

json MaxObject::to_json() const {
    std::lock_guard<std::mutex> lock(object_mutex);
    json j;
    j["id"] = id;
    j["type"] = type;
    j["position"] = {{"x", position.first}, {"y", position.second}};
    j["size"] = {{"width", size.first}, {"height", size.second}};
    
    json params_array = json::array();
    for (const auto& pair : parameters) {
        params_array.push_back(pair.second.to_json());
    }
    j["parameters"] = params_array;
    
    j["inlets"] = inlets;
    j["outlets"] = outlets;
    
    return j;
}

MaxObject MaxObject::from_json(const json& j) {
    MaxObject obj(j["id"].get<std::string>(), j["type"].get<std::string>());
    
    if (j.contains("position")) {
        obj.set_position(
            j["position"]["x"].get<int>(),
            j["position"]["y"].get<int>()
        );
    }
    
    if (j.contains("size")) {
        obj.set_size(
            j["size"]["width"].get<int>(),
            j["size"]["height"].get<int>()
        );
    }
    
    if (j.contains("parameters") && j["parameters"].is_array()) {
        for (const auto& param_json : j["parameters"]) {
            obj.add_parameter(Parameter::from_json(param_json));
        }
    }
    
    if (j.contains("inlets")) {
        obj.set_inlets(j["inlets"].get<int>());
    }
    
    if (j.contains("outlets")) {
        obj.set_outlets(j["outlets"].get<int>());
    }
    
    return obj;
}

// Connection implementation
Connection::Connection(const std::string& id, 
                       const std::string& source_id, int source_outlet,
                       const std::string& dest_id, int dest_inlet)
    : id(id), source_id(source_id), source_outlet(source_outlet),
      destination_id(dest_id), destination_inlet(dest_inlet) {
}

json Connection::to_json() const {
    json j;
    j["id"] = id;
    j["sourceId"] = source_id;
    j["sourceOutlet"] = source_outlet;
    j["destinationId"] = destination_id;
    j["destinationInlet"] = destination_inlet;
    return j;
}

Connection Connection::from_json(const json& j) {
    return Connection(
        j["id"].get<std::string>(),
        j["sourceId"].get<std::string>(),
        j["sourceOutlet"].get<int>(),
        j["destinationId"].get<std::string>(),
        j["destinationInlet"].get<int>()
    );
}

// Patch implementation
Patch::Patch(const std::string& id, const std::string& name, const std::string& path)
    : id(id), name(name), path(path), modified(false) {
}

void Patch::set_name(const std::string& new_name) {
    std::lock_guard<std::mutex> lock(patch_mutex);
    name = new_name;
    modified = true;
}

void Patch::set_path(const std::string& new_path) {
    std::lock_guard<std::mutex> lock(patch_mutex);
    path = new_path;
}

void Patch::set_modified(bool is_modified) {
    std::lock_guard<std::mutex> lock(patch_mutex);
    modified = is_modified;
}

void Patch::add_object(std::shared_ptr<MaxObject> object) {
    std::lock_guard<std::mutex> lock(patch_mutex);
    objects[object->get_id()] = object;
    modified = true;
}

void Patch::remove_object(const std::string& object_id) {
    std::lock_guard<std::mutex> lock(patch_mutex);
    objects.erase(object_id);
    modified = true;
}

bool Patch::has_object(const std::string& object_id) const {
    std::lock_guard<std::mutex> lock(patch_mutex);
    return objects.find(object_id) != objects.end();
}

std::shared_ptr<MaxObject> Patch::get_object(const std::string& object_id) const {
    std::lock_guard<std::mutex> lock(patch_mutex);
    auto it = objects.find(object_id);
    if (it == objects.end()) {
        throw std::runtime_error("Object not found: " + object_id);
    }
    return it->second;
}

void Patch::add_connection(std::shared_ptr<Connection> connection) {
    std::lock_guard<std::mutex> lock(patch_mutex);
    connections[connection->get_id()] = connection;
    modified = true;
}

void Patch::remove_connection(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(patch_mutex);
    connections.erase(connection_id);
    modified = true;
}

bool Patch::has_connection(const std::string& connection_id) const {
    std::lock_guard<std::mutex> lock(patch_mutex);
    return connections.find(connection_id) != connections.end();
}

std::shared_ptr<Connection> Patch::get_connection(const std::string& connection_id) const {
    std::lock_guard<std::mutex> lock(patch_mutex);
    auto it = connections.find(connection_id);
    if (it == connections.end()) {
        throw std::runtime_error("Connection not found: " + connection_id);
    }
    return it->second;
}

json Patch::to_json() const {
    std::lock_guard<std::mutex> lock(patch_mutex);
    json j;
    j["id"] = id;
    j["name"] = name;
    j["path"] = path;
    j["isModified"] = modified;
    
    json objects_array = json::array();
    for (const auto& pair : objects) {
        objects_array.push_back(pair.second->to_json());
    }
    j["objects"] = objects_array;
    
    json connections_array = json::array();
    for (const auto& pair : connections) {
        connections_array.push_back(pair.second->to_json());
    }
    j["connections"] = connections_array;
    
    return j;
}

Patch Patch::from_json(const json& j) {
    Patch patch(
        j["id"].get<std::string>(),
        j["name"].get<std::string>(),
        j["path"].get<std::string>()
    );
    
    patch.set_modified(j["isModified"].get<bool>());
    
    if (j.contains("objects") && j["objects"].is_array()) {
        for (const auto& obj_json : j["objects"]) {
            patch.add_object(std::make_shared<MaxObject>(MaxObject::from_json(obj_json)));
        }
    }
    
    if (j.contains("connections") && j["connections"].is_array()) {
        for (const auto& conn_json : j["connections"]) {
            patch.add_connection(std::make_shared<Connection>(Connection::from_json(conn_json)));
        }
    }
    
    return patch;
}

// GlobalSettings implementation
void GlobalSettings::set_setting(const std::string& name, const json& value) {
    std::lock_guard<std::mutex> lock(settings_mutex);
    settings[name] = value;
}

const json& GlobalSettings::get_setting(const std::string& name) const {
    std::lock_guard<std::mutex> lock(settings_mutex);
    auto it = settings.find(name);
    if (it == settings.end()) {
        throw std::runtime_error("Setting not found: " + name);
    }
    return it->second;
}

bool GlobalSettings::has_setting(const std::string& name) const {
    std::lock_guard<std::mutex> lock(settings_mutex);
    return settings.find(name) != settings.end();
}

json GlobalSettings::to_json() const {
    std::lock_guard<std::mutex> lock(settings_mutex);
    json j = json::object();
    for (const auto& pair : settings) {
        j[pair.first] = pair.second;
    }
    return j;
}

GlobalSettings GlobalSettings::from_json(const json& j) {
    GlobalSettings settings;
    for (auto it = j.begin(); it != j.end(); ++it) {
        settings.set_setting(it.key(), it.value());
    }
    return settings;
}

// Session implementation
Session::Session(const std::string& id, const std::string& name)
    : id(id), name(name) {
    // Set start time to current time in milliseconds since epoch
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    start_time = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

void Session::set_name(const std::string& new_name) {
    std::lock_guard<std::mutex> lock(session_mutex);
    name = new_name;
}

void Session::add_patch(std::shared_ptr<Patch> patch) {
    std::lock_guard<std::mutex> lock(session_mutex);
    patches[patch->get_id()] = patch;
}

void Session::remove_patch(const std::string& patch_id) {
    std::lock_guard<std::mutex> lock(session_mutex);
    patches.erase(patch_id);
}

bool Session::has_patch(const std::string& patch_id) const {
    std::lock_guard<std::mutex> lock(session_mutex);
    return patches.find(patch_id) != patches.end();
}

std::shared_ptr<Patch> Session::get_patch(const std::string& patch_id) const {
    std::lock_guard<std::mutex> lock(session_mutex);
    auto it = patches.find(patch_id);
    if (it == patches.end()) {
        throw std::runtime_error("Patch not found: " + patch_id);
    }
    return it->second;
}

json Session::to_json() const {
    std::lock_guard<std::mutex> lock(session_mutex);
    json j;
    j["id"] = id;
    j["name"] = name;
    j["startTime"] = start_time;
    
    json patches_array = json::array();
    for (const auto& pair : patches) {
        patches_array.push_back(pair.second->to_json());
    }
    j["patches"] = patches_array;
    
    j["globalSettings"] = global_settings.to_json();
    
    return j;
}

Session Session::from_json(const json& j) {
    Session session(
        j["id"].get<std::string>(),
        j["name"].get<std::string>()
    );
    
    if (j.contains("startTime")) {
        session.start_time = j["startTime"].get<long long>();
    }
    
    if (j.contains("patches") && j["patches"].is_array()) {
        for (const auto& patch_json : j["patches"]) {
            session.add_patch(std::make_shared<Patch>(Patch::from_json(patch_json)));
        }
    }
    
    if (j.contains("globalSettings")) {
        session.global_settings = GlobalSettings::from_json(j["globalSettings"]);
    }
    
    return session;
}

// StateEvent implementation
StateEvent::StateEvent(Category category, EventType type, const std::string& object_id, 
                     const json& data, long long timestamp)
    : category(category), event_type(type), object_id(object_id), data(data) {
    
    if (timestamp == 0) {
        // Set timestamp to current time in milliseconds since epoch
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        this->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    } else {
        this->timestamp = timestamp;
    }
}

json StateEvent::to_json() const {
    json j;
    j["category"] = category_to_string(category);
    j["eventType"] = event_type_to_string(event_type);
    j["objectId"] = object_id;
    j["data"] = data;
    j["timestamp"] = timestamp;
    return j;
}

StateEvent StateEvent::from_json(const json& j) {
    return StateEvent(
        string_to_category(j["category"].get<std::string>()),
        string_to_event_type(j["eventType"].get<std::string>()),
        j["objectId"].get<std::string>(),
        j["data"],
        j["timestamp"].get<long long>()
    );
}

std::string StateEvent::category_to_string(Category category) {
    switch (category) {
        case Category::Session: return "session";
        case Category::Patch: return "patch";
        case Category::Object: return "object";
        case Category::Parameter: return "parameter";
        case Category::Connection: return "connection";
        case Category::GlobalSetting: return "globalSetting";
        default: return "unknown";
    }
}

std::string StateEvent::event_type_to_string(EventType type) {
    switch (type) {
        case EventType::Created: return "created";
        case EventType::Updated: return "updated";
        case EventType::Deleted: return "deleted";
        case EventType::Connected: return "connected";
        case EventType::Disconnected: return "disconnected";
        case EventType::Moved: return "moved";
        case EventType::Resized: return "resized";
        case EventType::ParamChanged: return "paramChanged";
        case EventType::StateChanged: return "stateChanged";
        default: return "unknown";
    }
}

StateEvent::Category StateEvent::string_to_category(const std::string& str) {
    if (str == "session") return Category::Session;
    if (str == "patch") return Category::Patch;
    if (str == "object") return Category::Object;
    if (str == "parameter") return Category::Parameter;
    if (str == "connection") return Category::Connection;
    if (str == "globalSetting") return Category::GlobalSetting;
    throw std::runtime_error("Unknown category: " + str);
}

StateEvent::EventType StateEvent::string_to_event_type(const std::string& str) {
    if (str == "created") return EventType::Created;
    if (str == "updated") return EventType::Updated;
    if (str == "deleted") return EventType::Deleted;
    if (str == "connected") return EventType::Connected;
    if (str == "disconnected") return EventType::Disconnected;
    if (str == "moved") return EventType::Moved;
    if (str == "resized") return EventType::Resized;
    if (str == "paramChanged") return EventType::ParamChanged;
    if (str == "stateChanged") return EventType::StateChanged;
    throw std::runtime_error("Unknown event type: " + str);
}

// StateChange implementation
StateChange::StateChange(const StateEvent& event)
    : event(event) {
}

StateChange::StateChange(StateEvent::Category category, StateEvent::EventType type, 
                       const std::string& object_id, const json& data)
    : event(category, type, object_id, data) {
}

json StateChange::to_notification_json() const {
    return event.to_json();
}

std::string StateChange::to_osc_address() const {
    std::ostringstream oss;
    oss << "/max/state/" 
        << StateEvent::category_to_string(event.get_category()) << "/"
        << StateEvent::event_type_to_string(event.get_event_type());
    return oss.str();
}

// StateDiff implementation
StateDiff::StateDiff(Operation op, const std::string& path, const json& value)
    : operation(op), path(path), value(value) {
}

json StateDiff::to_json() const {
    json j;
    j["op"] = operation_to_string(operation);
    j["path"] = path;
    if (operation != Operation::Remove) {
        j["value"] = value;
    }
    return j;
}

StateDiff StateDiff::from_json(const json& j) {
    Operation op = string_to_operation(j["op"].get<std::string>());
    std::string path = j["path"].get<std::string>();
    
    if (op == Operation::Remove) {
        return StateDiff(op, path);
    } else {
        return StateDiff(op, path, j["value"]);
    }
}

std::string StateDiff::operation_to_string(Operation op) {
    switch (op) {
        case Operation::Add: return "add";
        case Operation::Replace: return "replace";
        case Operation::Remove: return "remove";
        case Operation::Move: return "move";
        default: return "unknown";
    }
}

StateDiff::Operation StateDiff::string_to_operation(const std::string& str) {
    if (str == "add") return Operation::Add;
    if (str == "replace") return Operation::Replace;
    if (str == "remove") return Operation::Remove;
    if (str == "move") return Operation::Move;
    throw std::runtime_error("Unknown operation: " + str);
}

} // namespace state
} // namespace mcp
