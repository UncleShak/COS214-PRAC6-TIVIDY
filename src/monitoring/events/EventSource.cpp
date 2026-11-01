/**
 * @file EventSource.cpp
 * @brief Implementation of EventSource.
 *
 * @author Sloth (The Archivist)
 */
#include "EventSource.h"

void EventSource::attach(EventListener* listener) {
    if (listener == nullptr) {
        return;
    }
    auto it = std::find(listeners.begin(), listeners.end(), listener);
    if (it == listeners.end()) {
        listeners.push_back(listener);
    }
}

void EventSource::detach(EventListener* listener) {
    if (listener == nullptr) {
        return;
    }
    listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
}

void EventSource::notifyListeners(const Event& event) {
    for (EventListener* listener : listeners) {
        if (listener != nullptr) {
            listener->onEvent(event);
        }
    }
}

std::size_t EventSource::getListenerCount() const {
    return listeners.size();
}
