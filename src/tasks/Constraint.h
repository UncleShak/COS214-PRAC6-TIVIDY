/**
 * @file Constraint.h
 * @brief A rule that must hold before a work item may be performed. Used by ValidationDecorator.
 *
 * The class diagram names the type Constraint but does not define it (D-G7). Each industry
 * supplies its own constraints, for example "all required documents uploaded".
 *
 * @author N'des Junior Lungwangu (Gluttony)
 */
#ifndef TIVIDY_CONSTRAINT_H
#define TIVIDY_CONSTRAINT_H

#include <string>

/** @brief Precondition checked before work is performed. */
class Constraint {
public:
    virtual ~Constraint() {}
    /** @brief True if the work may go ahead. */
    virtual bool isSatisfied() const = 0;
    /** @brief Explains the rule, used in the error message when it fails. */
    virtual std::string describe() const = 0;
};

#endif
