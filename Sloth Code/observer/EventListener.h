/**
 * @file EventListener.h
 * @brief Abstract Observer interface for the Observer pattern.
 *
 * Pattern: Observer (Observer role).
 * Components that need to react to workflow lifecycle events (such as
 * history recorders, notification dispatchers, or monitoring agents)
 * implement this interface.
 *
 * @author Sloth (The Archivist)
 */
#ifndef TIVIDY_EVENT_LISTENER_H
#define TIVIDY_EVENT_LISTENER_H

class Event;

class EventListener {
public:
    virtual ~EventListener() = default;

    /**
     * @brief Called by an EventSource when a workflow event occurs.
     * @param event The event being dispatched.
     */
    virtual void onEvent(const Event& event) = 0;
};

#endif // TIVIDY_EVENT_LISTENER_H
