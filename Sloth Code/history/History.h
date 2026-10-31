/**
 * @file History.h
 * @brief Abstract interface for history and audit recording.
 *
 * Implements the contract required by AuditDecorator (D-G7).
 * Receives audit entries in chronological order.
 *
 * @author Sloth (The Archivist) / Gluttony
 */
#ifndef TIVIDY_HISTORY_H
#define TIVIDY_HISTORY_H

#include <string>

class History {
public:
    virtual ~History() = default;

    /**
     * @brief Appends one audit entry to the log.
     * @param entry Text describing what occurred.
     */
    virtual void record(const std::string& entry) = 0;
};

#endif // TIVIDY_HISTORY_H
