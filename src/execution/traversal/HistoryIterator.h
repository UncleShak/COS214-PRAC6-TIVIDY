/**
 * @file HistoryIterator.h
 * @brief ConcreteIterator: steps through an instance's history entries.
 *
 * Works over any ordered list of history entries, so it fits the history the team
 * already records: CommandLog::describeAll() (Gluttony's Command invoker), the entries an
 * AuditDecorator writes, and Sloth's history class once it exists. The interface can show
 * history oldest first or newest first, and narrow it to one work item.
 *
 * It copies the entries when created, so new history recorded while looping does not
 * disturb the traversal.
 *
 * @author Ona Mojela (Envy)
 */
#ifndef TIVIDY_HISTORY_ITERATOR_H
#define TIVIDY_HISTORY_ITERATOR_H

#include <string>
#include <vector>

#include "WorkflowIterator.h"

/** @brief Iterates over history entries in either order, optionally filtered. */
class HistoryIterator : public WorkflowIterator<std::string> {
public:
    /** @brief Traversal order. */
    enum Order { OldestFirst, NewestFirst };

    /**
     * @param entries History entries, oldest first (the order they were recorded).
     * @param order   OldestFirst (default) or NewestFirst.
     * @param filter  If not empty, only entries containing this text are returned,
     *                e.g. a work item's name to see just that item's history.
     */
    explicit HistoryIterator(const std::vector<std::string>& entries,
                             Order order = OldestFirst,
                             const std::string& filter = "");

    void first() override;
    void next() override;
    bool isDone() const override;
    std::string currentItem() const override;

    /** @brief Number of entries this iterator will return (after filtering). */
    std::size_t size() const { return selected.size(); }

private:
    std::vector<std::string> selected;   ///< already filtered and in traversal order
    std::size_t position;
};

#endif
