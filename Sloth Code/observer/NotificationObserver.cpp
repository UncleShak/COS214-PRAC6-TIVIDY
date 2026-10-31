/**
 * @file NotificationObserver.cpp
 * @brief Implementation of NotificationObserver.
 *
 * @author Sloth (The Archivist)
 */
#include "NotificationObserver.h"
#include <sstream>

void NotificationObserver::onEvent(const Event& event) {
    std::ostringstream oss;
    oss << "[ALERT] " << event.getType();
    if (!event.getWorkItemName().empty()) {
        oss << " for '" << event.getWorkItemName() << "'";
    }
    if (!event.getNewState().empty()) {
        oss << " -> State: " << event.getNewState();
    }
    if (!event.getDescription().empty()) {
        oss << " (" << event.getDescription() << ")";
    }
    notifications.push_back(oss.str());
}

const std::vector<std::string>& NotificationObserver::getNotifications() const {
    return notifications;
}

void NotificationObserver::clearNotifications() {
    notifications.clear();
}

std::size_t NotificationObserver::getNotificationCount() const {
    return notifications.size();
}
