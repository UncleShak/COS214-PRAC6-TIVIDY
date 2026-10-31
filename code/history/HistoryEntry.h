/**
 * @file HistoryEntry.h
 * @brief Represents an individual immutable record in an instance's execution log.
 *
 * Stores event metadata including timestamp, action/event type, work item name,
 * state change (if applicable), and diagnostic details.
 *
 * @author Sloth (The Archivist)
 */
#ifndef TIVIDY_HISTORY_ENTRY_H
#define TIVIDY_HISTORY_ENTRY_H

#include <string>
#include <ctime>

class HistoryEntry {
public:
    HistoryEntry(const std::string& type,
                 const std::string& workItemName,
                 const std::string& oldState,
                 const std::string& newState,
                 const std::string& details,
                 std::time_t timestamp = 0);

    explicit HistoryEntry(const std::string& textMessage, std::time_t timestamp = 0);

    ~HistoryEntry() = default;

    std::string getType() const;
    std::string getWorkItemName() const;
    std::string getOldState() const;
    std::string getNewState() const;
    std::string getDetails() const;
    std::time_t getTimestamp() const;

    /**
     * @brief Formats this history entry as a human-readable log string.
     */
    std::string toString() const;

private:
    std::string type;
    std::string workItemName;
    std::string oldState;
    std::string newState;
    std::string details;
    std::time_t timestamp;
};

#endif // TIVIDY_HISTORY_ENTRY_H
