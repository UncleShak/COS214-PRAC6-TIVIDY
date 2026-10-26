/**
 * @file Participant.h
 * @brief STAND-IN for the Participant class (owned by Greed), with the attributes and
 *        operation from the Practical 6 class diagram.
 *
 * @author Tanatswa Masiiwa (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_PARTICIPANT_H
#define TIVIDY_PARTICIPANT_H

#include <string>
#include <vector>

/** @brief A person (or system) who can be given work and act on it. */
class Participant {
public:
    /**
     * @param participantID Unique identifier.
     * @param name Display name.
     * @param roles Role names this participant holds.
     */
    Participant(const std::string& participantID, const std::string& name,
                const std::vector<std::string>& roles);

    /** @brief True if this participant holds the given role. */
    bool hasRole(const std::string& roleName) const;

    const std::string& getID() const { return participantID; }
    const std::string& getName() const { return name; }

private:
    std::string participantID;
    std::string name;
    std::vector<std::string> roles;
};

#endif
