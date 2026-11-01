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

#include <climits>
#include <stdexcept>

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
    /**
     * @brief Moves time forward by the given number of units.
     * @throws std::invalid_argument if units is negative (time never goes back).
     * @throws std::overflow_error if the time would pass INT_MAX (bug 20: it used to wrap negative).
     */
    void advance(int units) {
        if (units < 0) throw std::invalid_argument("The clock only moves forward");
        if (current > INT_MAX - units) throw std::overflow_error("The clock cannot move that far forward");
        current += units;
    }

private:
    int current;
};

#endif
