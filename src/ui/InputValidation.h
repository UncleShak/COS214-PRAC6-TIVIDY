/**
 * @file InputValidation.h
 * @brief Checks the user's free text before it reaches the engine.
 *
 * Pure functions with no I/O, shared by both front ends and covered by
 * tests/ui/test_validation.cpp.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_INPUT_VALIDATION_H
#define TIVIDY_INPUT_VALIDATION_H

#include <cstddef>
#include <string>

#include "EngineGateway.h"

namespace tividy {
namespace ui {

/** @brief Longest free-text value accepted (characters, after trimming). */
const std::size_t kMaxTextLength = 200;

/** @brief Removes leading and trailing whitespace. */
std::string trim(const std::string& text);

/**
 * @brief True if @p text is a number by the same rule as KeyValuePayload::getNumber:
 *        strtod consumes the whole (trimmed) string, which must not be empty.
 */
bool isNumber(const std::string& text);

/** @brief Parses yes/no, y/n, true/false (any case). @return false if it is none of them. */
bool parseYesNo(const std::string& text, bool& value);

/**
 * @brief Parses a 1-based menu choice typed by the user.
 * @param count number of menu entries
 * @param index set to the 0-based index on success
 * @return false if not a whole number in 1..count
 */
bool parseMenuChoice(const std::string& text, std::size_t count, std::size_t& index);

/**
 * @brief Validates one start-instance field.
 * @param raw     what the user typed
 * @param cleaned set to the value to send (trimmed, or the default for an empty optional field)
 * @return an empty string if valid, otherwise a message for the user
 */
std::string validateField(const DataField& field, const std::string& raw, std::string& cleaned);

} // namespace ui
} // namespace tividy

#endif
