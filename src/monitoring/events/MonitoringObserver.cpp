/**
 * @file MonitoringObserver.cpp
 * @brief Implementation of MonitoringObserver.
 *
 * @author Sloth (The Archivist)
 */
#include "MonitoringObserver.h"

void MonitoringObserver::onEvent(const Event& event) {
    ++totalEvents;
    eventTypeCounts[event.getType()]++;
    if (!event.getNewState().empty()) {
        stateCounts[event.getNewState()]++;
    }
}

int MonitoringObserver::getEventCount(const std::string& eventType) const {
    auto it = eventTypeCounts.find(eventType);
    if (it != eventTypeCounts.end()) {
        return it->second;
    }
    return 0;
}

int MonitoringObserver::getStateTransitionCount(const std::string& stateName) const {
    auto it = stateCounts.find(stateName);
    if (it != stateCounts.end()) {
        return it->second;
    }
    return 0;
}

int MonitoringObserver::getTotalEventCount() const {
    return totalEvents;
}

void MonitoringObserver::reset() {
    totalEvents = 0;
    eventTypeCounts.clear();
    stateCounts.clear();
}
