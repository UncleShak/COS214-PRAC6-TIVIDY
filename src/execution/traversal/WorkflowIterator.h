/**
 * @file WorkflowIterator.h
 * @brief Iterator participant: steps through a collection of workflow items one at a time.
 *
 * The interface, reports and the engine need to walk through available work and through
 * an instance's history without knowing how either is stored (vector, set, log class).
 * Each collection gives out a ConcreteIterator, and callers only use this interface:
 *
 * @code
 * for (it->first(); !it->isDone(); it->next()) {
 *     show(it->currentItem());
 * }
 * @endcode
 *
 * GoF roles: Iterator = WorkflowIterator; ConcreteIterator = AvailableWorkIterator,
 * HistoryIterator; Aggregate = ProgressionTracker (available work) and the history entries
 * (CommandLog::describeAll(), Sloth's history); createIterator() is the factory method.
 *
 * @tparam T What the iterator returns (work-item names and history entries are strings).
 *
 * @author Ona Mojela (Envy)
 */
#ifndef TIVIDY_WORKFLOW_ITERATOR_H
#define TIVIDY_WORKFLOW_ITERATOR_H

/** @brief GoF Iterator interface: first, next, isDone, currentItem. */
template <typename T>
class WorkflowIterator {
public:
    virtual ~WorkflowIterator() = default;

    /** @brief Moves to the first element (also restarts a finished traversal). */
    virtual void first() = 0;
    /** @brief Moves to the next element. Does nothing once isDone() is true. */
    virtual void next() = 0;
    /** @brief True when there are no more elements. */
    virtual bool isDone() const = 0;
    /**
     * @brief The element at the current position.
     * @throws std::out_of_range if isDone() is true.
     */
    virtual T currentItem() const = 0;
};

#endif
