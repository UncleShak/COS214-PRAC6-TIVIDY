/**
 * @file History.h
 * @brief Where AuditDecorator writes its audit entries.
 *
 * The class diagram types AuditDecorator::history as History* but does not define it (D-G7).
 * This small interface keeps the decorator independent of how history is stored: Sloth's
 * history and audit class can implement it directly, or through an adapter.
 *
 * @author Tanatswa Masiiwa (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_HISTORY_H
#define TIVIDY_HISTORY_H

#include <string>

/** @brief Receives audit entries in the order they happen. */
class History {
public:
    virtual ~History() {}
    /** @brief Appends one audit entry. */
    virtual void record(const std::string& entry) = 0;
};

#endif
