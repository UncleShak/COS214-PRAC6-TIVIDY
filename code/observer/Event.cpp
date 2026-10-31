/**
 * @file Event.cpp
 * @brief Implementation of Event.
 *
 * @author Sloth (The Archivist)
 */
#include "Event.h"
#include <sstream>
#include <iomanip>

Event::Event(const std::string& type,
             const std::string& workItemName,
             const std::string& oldState,
             const std::string& newState,
             const std::string& description)
    : type(type),
      workItemName(workItemName),
      oldState(oldState),
      newState(newState),
      description(description),
      timestamp(std::time(nullptr)) {
}

Event::Event(const std::string& type, const WorkItem* /*source*/)
    : type(type),
      workItemName(""),
      oldState(""),
      newState(""),
      description("State changed on work item"),
      timestamp(std::time(nullptr)) {
}

Event::Event(const std::string& type, const std::string& description)
    : type(type),
      workItemName(""),
      oldState(""),
      newState(""),
      description(description),
      timestamp(std::time(nullptr)) {
}

std::string Event::getType() const {
    return type;
}

std::string Event::getWorkItemName() const {
    return workItemName;
}

std::string Event::getOldState() const {
    return oldState;
}

std::string Event::getNewState() const {
    return newState;
}

std::string Event::getDescription() const {
    return description;
}

std::time_t Event::getTimestamp() const {
    return timestamp;
}

std::string Event::toString() const {
    std::ostringstream oss;
    oss << "[" << type << "]";
    if (!workItemName.empty()) {
        oss << " Item: " << workItemName;
    }
    if (!oldState.empty() || !newState.empty()) {
        oss << " (" << oldState << " -> " << newState << ")";
    }
    if (!description.empty()) {
        oss << " - " << description;
    }
    return oss.str();
}
