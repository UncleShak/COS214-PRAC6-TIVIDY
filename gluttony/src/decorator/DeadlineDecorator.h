/**
 * @file DeadlineDecorator.h
 * @brief ConcreteDecorator: gives work a due time and refuses late work so it can be escalated.
 *
 * @author Tanatswa Masiiwa (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_DEADLINE_DECORATOR_H
#define TIVIDY_DEADLINE_DECORATOR_H

#include "TaskDecorator.h"

class Clock;

/** @brief Adds a deadline: dueTime = time of decoration + duration. */
class DeadlineDecorator : public TaskDecorator {
public:
    /**
     * @param c The component to decorate.
     * @param duration How long the work may take, in clock units. Must be at least 0.
     * @param clock Where the current time is read from (not owned).
     * @throws std::invalid_argument if clock is null or duration is negative.
     */
    DeadlineDecorator(WorkflowComponent* c, int duration, const Clock* clock);

    /**
     * @brief Performs the work if it is still on time.
     * @throws DeadlineExceeded if the due time has passed. The work does not run, so the
     *         engine's escalation (Chain of Responsibility) can pass it to the next level.
     */
    void request() override;

    /** @brief True once the current time is later than the due time. */
    bool isOverdue() const;

    int getDuration() const { return duration; }
    int getDueTime() const { return dueTime; }

private:
    int duration;
    int dueTime;
    const Clock* clock;
};

#endif
