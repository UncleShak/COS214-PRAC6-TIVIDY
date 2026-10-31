/**
 * @file MonitoringObserver.h
 * @brief Concrete Observer that records metrics and monitoring statistics.
 *
 * Pattern: Observer (ConcreteObserver role).
 * Counts transition frequencies and event types for system observability.
 *
 * @author Sloth (The Archivist)
 */
#ifndef TIVIDY_MONITORING_OBSERVER_H
#define TIVIDY_MONITORING_OBSERVER_H

#include "EventListener.h"
#include "Event.h"
#include <map>
#include <string>

class MonitoringObserver : public EventListener {
public:
    MonitoringObserver() : totalEvents(0) {}
    ~MonitoringObserver() override = default;

    void onEvent(const Event& event) override;

    int getEventCount(const std::string& eventType) const;
    int getStateTransitionCount(const std::string& stateName) const;
    int getTotalEventCount() const;
    void reset();

private:
    int totalEvents;
    std::map<std::string, int> eventTypeCounts;
    std::map<std::string, int> stateCounts;
};

#endif // TIVIDY_MONITORING_OBSERVER_H
