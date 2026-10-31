/**
 * @file InstanceHistory.cpp
 * @brief Implementation of InstanceHistory.
 *
 * @author Sloth (The Archivist)
 */
#include "InstanceHistory.h"

InstanceHistory::InstanceHistory(const std::string& instanceId)
    : instanceId(instanceId) {
}

void InstanceHistory::record(const std::string& entry) {
    entries.emplace_back(entry);
}

void InstanceHistory::onEvent(const Event& event) {
    entries.emplace_back(
        event.getType(),
        event.getWorkItemName(),
        event.getOldState(),
        event.getNewState(),
        event.getDescription(),
        event.getTimestamp()
    );
}

void InstanceHistory::addEntry(const HistoryEntry& entry) {
    entries.push_back(entry);
}

const std::vector<HistoryEntry>& InstanceHistory::getEntries() const {
    return entries;
}

std::vector<HistoryEntry> InstanceHistory::getEntriesFor(const std::string& workItemName) const {
    std::vector<HistoryEntry> filtered;
    for (const auto& entry : entries) {
        if (entry.getWorkItemName() == workItemName) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

std::vector<std::string> InstanceHistory::toStringList() const {
    std::vector<std::string> list;
    list.reserve(entries.size());
    for (const auto& entry : entries) {
        list.push_back(entry.toString());
    }
    return list;
}

std::vector<std::string> InstanceHistory::toStringListFor(const std::string& workItemName) const {
    std::vector<std::string> list;
    for (const auto& entry : entries) {
        if (entry.getWorkItemName() == workItemName) {
            list.push_back(entry.toString());
        }
    }
    return list;
}

std::string InstanceHistory::getInstanceId() const {
    return instanceId;
}

std::size_t InstanceHistory::size() const {
    return entries.size();
}

bool InstanceHistory::empty() const {
    return entries.empty();
}

void InstanceHistory::clear() {
    entries.clear();
}
