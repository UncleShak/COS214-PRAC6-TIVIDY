/**
 * @file Clock.h
 * @brief Source of the current time for DeadlineDecorator.
 *
 * The class diagram gives DeadlineDecorator isOverdue() with no parameter, so the decorator
 * needs somewhere to read "now" from (D-G8). Injecting a Clock keeps time in whole units
 * (the diagram uses int) and lets tests control time exactly.
 *
 * @author N'des Junior Lungwangu (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_CLOCK_H
#define TIVIDY_CLOCK_H

/** @brief Returns the current time in whole units (for example minutes or simulation ticks). */
class Clock {
public:
    virtual ~Clock() {}
    virtual int now() const = 0;
};

/** @brief A clock that only moves when told to: used by tests and the interactive demo. */
class ManualClock : public Clock {
public:
    explicit ManualClock(int start = 0) : current(start) {}
    int now() const override { return current; }
    /** @brief Moves time forward by the given number of units. */
    void advance(int units) { current += units; }

private:
    int current;
};

#endif
