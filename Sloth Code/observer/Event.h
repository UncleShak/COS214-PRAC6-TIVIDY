/**
 * @file Event.h
 * @brief Encapsulates a workflow lifecycle event.
 *
 * Pattern: Observer (Notification object).
 * Represents events such as state changes, assignments, cancellations,
 * or escalations in the workflow engine.
 *
 * @author Sloth (The Archivist)
 */
#ifndef TIVIDY_EVENT_H
#define TIVIDY_EVENT_H

#include <string>
#include <ctime>

class WorkItem;

class Event {
public:
    /**
     * @brief Detailed constructor for state changes.
     */
    Event(const std::string& type,
          const std::string& workItemName,
          const std::string& oldState,
          const std::string& newState,
          const std::string& description = "");

    /**
     * @brief Compatibility constructor matching WorkItem.cpp usage:
     *        notifyListeners(new Event("STATE_CHANGED", this));
     */
    Event(const std::string& type, const WorkItem* source);

    /**
     * @brief Simple event constructor.
     */
    Event(const std::string& type, const std::string& description);

    ~Event() = default;

    std::string getType() const;
    std::string getWorkItemName() const;
    std::string getOldState() const;
    std::string getNewState() const;
    std::string getDescription() const;
    std::time_t getTimestamp() const;

    /**
     * @brief Formats the event as a readable log line.
     */
    std::string toString() const;

private:
    std::string type;
    std::string workItemName;
    std::string oldState;
    std::string newState;
    std::string description;
    std::time_t timestamp;
};

#endif // TIVIDY_EVENT_H
