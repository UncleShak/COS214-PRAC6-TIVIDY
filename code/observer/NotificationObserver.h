/**
 * @file NotificationObserver.h
 * @brief Concrete Observer that dispatches alerts/notifications for events.
 *
 * Pattern: Observer (ConcreteObserver role).
 * Collects notifications for critical transitions (e.g. failure, rejection, escalation)
 * so stakeholders or system administrators are informed.
 *
 * @author Sloth (The Archivist)
 */
#ifndef TIVIDY_NOTIFICATION_OBSERVER_H
#define TIVIDY_NOTIFICATION_OBSERVER_H

#include "EventListener.h"
#include "Event.h"
#include <vector>
#include <string>

class NotificationObserver : public EventListener {
public:
    NotificationObserver() = default;
    ~NotificationObserver() override = default;

    void onEvent(const Event& event) override;

    const std::vector<std::string>& getNotifications() const;
    void clearNotifications();
    std::size_t getNotificationCount() const;

private:
    std::vector<std::string> notifications;
};

#endif // TIVIDY_NOTIFICATION_OBSERVER_H
