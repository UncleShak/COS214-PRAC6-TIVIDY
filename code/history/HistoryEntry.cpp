/**
 * @file HistoryEntry.cpp
 * @brief Implementation of HistoryEntry.
 *
 * @author Sloth (The Archivist)
 */
#include "HistoryEntry.h"
#include <sstream>

HistoryEntry::HistoryEntry(const std::string& type,
                           const std::string& workItemName,
                           const std::string& oldState,
                           const std::string& newState,
                           const std::string& details,
                           std::time_t timestamp)
    : type(type),
      workItemName(workItemName),
      oldState(oldState),
      newState(newState),
      details(details),
      timestamp(timestamp == 0 ? std::time(nullptr) : timestamp) {
}

HistoryEntry::HistoryEntry(const std::string& textMessage, std::time_t timestamp)
    : type("AUDIT"),
      workItemName(""),
      oldState(""),
      newState(""),
      details(textMessage),
      timestamp(timestamp == 0 ? std::time(nullptr) : timestamp) {
}

std::string HistoryEntry::getType() const {
    return type;
}

std::string HistoryEntry::getWorkItemName() const {
    return workItemName;
}

std::string HistoryEntry::getOldState() const {
    return oldState;
}

std::string HistoryEntry::getNewState() const {
    return newState;
}

std::string HistoryEntry::getDetails() const {
    return details;
}

std::time_t HistoryEntry::getTimestamp() const {
    return timestamp;
}

std::string HistoryEntry::toString() const {
    std::ostringstream oss;
    oss << "[" << type << "]";
    if (!workItemName.empty()) {
        oss << " Item: " << workItemName;
    }
    if (!oldState.empty() || !newState.empty()) {
        oss << " (" << oldState << " -> " << newState << ")";
    }
    if (!details.empty()) {
        oss << " - " << details;
    }
    return oss.str();
}
