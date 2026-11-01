/**
 * @file AvailableWorkIterator.h
 * @brief ConcreteIterator: the work an instance has available right now.
 *
 * Created by ProgressionTracker::createIterator(). It iterates over a copy of the
 * available list taken when it was created, so completing work while looping (which
 * changes the tracker's list) cannot break the traversal. Call createIterator() again
 * to see the new list.
 *
 * Items come out in the order they became available. The instance maps each name to its
 * WorkItem (WorkItem::getName()).
 *
 * @author Ona Mojela (Envy)
 */
#ifndef TIVIDY_AVAILABLE_WORK_ITERATOR_H
#define TIVIDY_AVAILABLE_WORK_ITERATOR_H

#include <string>
#include <vector>

#include "WorkflowIterator.h"

/** @brief Iterates over the names of available work items. */
class AvailableWorkIterator : public WorkflowIterator<std::string> {
public:
    /** @param availableItems The available work, in the order it became available. */
    explicit AvailableWorkIterator(const std::vector<std::string>& availableItems);

    void first() override;
    void next() override;
    bool isDone() const override;
    std::string currentItem() const override;

private:
    std::vector<std::string> items;
    std::size_t position;
};

#endif
