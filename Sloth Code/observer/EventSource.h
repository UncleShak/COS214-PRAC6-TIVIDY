/**
 * @file EventSource.h
 * @brief Subject in the Observer pattern for event notification.
 *
 * Pattern: Observer (Subject role).
 * Holds a collection of EventListener observers and notifies them when events occur.
 *
 * @author Sloth (The Archivist)
 */
#ifndef TIVIDY_EVENT_SOURCE_H
#define TIVIDY_EVENT_SOURCE_H

#include <vector>
#include <algorithm>
#include "EventListener.h"
#include "Event.h"

class EventSource {
public:
    virtual ~EventSource() = default;

    /**
     * @brief Attach a listener to receive event notifications.
     * @param listener The observer to attach (not owned).
     */
    virtual void attach(EventListener* listener);

    /**
     * @brief Detach a previously attached listener.
     * @param listener The observer to detach.
     */
    virtual void detach(EventListener* listener);

    /**
     * @brief Dispatch an event to all registered listeners.
     * @param event The event object being dispatched.
     */
    virtual void notifyListeners(const Event& event);

    // Convenience aliases
    void subscribe(EventListener* listener) { attach(listener); }
    void unsubscribe(EventListener* listener) { detach(listener); }

    /**
     * @brief Number of currently registered observers.
     */
    std::size_t getListenerCount() const;

private:
    std::vector<EventListener*> listeners;
};

#endif // TIVIDY_EVENT_SOURCE_H
