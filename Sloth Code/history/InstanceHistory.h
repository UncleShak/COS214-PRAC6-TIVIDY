/**
 * @file InstanceHistory.h
 * @brief Per-instance chronological execution history and audit log.
 *
 * Implements:
 * 1. History: Gluttony's interface, allowing AuditDecorator to write directly.
 * 2. EventListener: The Observer pattern, receiving notifications whenever work
 *    items change states, are assigned, completed, rejected, or escalated.
 *
 * Guarantees:
 * - Chronological ordering of all events.
 * - Strict instance isolation (each WorkflowInstance possesses its own independent
 *   InstanceHistory; changes in one never affect another).
 * - Direct compatibility with Envy's HistoryIterator via toStringList().
 *
 * @author Sloth (The Archivist)
 */
#ifndef TIVIDY_INSTANCE_HISTORY_H
#define TIVIDY_INSTANCE_HISTORY_H

#include <string>
#include <vector>
#include "History.h"
#include "EventListener.h"
#include "HistoryEntry.h"
#include "Event.h"

class InstanceHistory : public History, public EventListener {
public:
    explicit InstanceHistory(const std::string& instanceId = "");
    ~InstanceHistory() override = default;

    /**
     * @brief Implements History interface.
     * Records a free-form string entry (e.g. from AuditDecorator).
     */
    void record(const std::string& entry) override;

    /**
     * @brief Implements EventListener interface.
     * Receives structured workflow events and converts them into history entries.
     */
    void onEvent(const Event& event) override;

    /**
     * @brief Manually appends a structured entry.
     */
    void addEntry(const HistoryEntry& entry);

    /**
     * @brief Returns all history entries in chronological order.
     */
    const std::vector<HistoryEntry>& getEntries() const;

    /**
     * @brief Returns history filtered to a specific work item.
     */
    std::vector<HistoryEntry> getEntriesFor(const std::string& workItemName) const;

    /**
     * @brief Returns all entries formatted as strings, suitable for HistoryIterator.
     */
    std::vector<std::string> toStringList() const;

    /**
     * @brief Returns entries for a specific work item formatted as strings.
     */
    std::vector<std::string> toStringListFor(const std::string& workItemName) const;

    std::string getInstanceId() const;
    std::size_t size() const;
    bool empty() const;
    void clear();

private:
    std::string instanceId;
    std::vector<HistoryEntry> entries;
};

#endif // TIVIDY_INSTANCE_HISTORY_H
