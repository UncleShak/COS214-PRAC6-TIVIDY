#include "Participant.h"

#include <algorithm>

Participant::Participant(const std::string& participantID, const std::string& name,
                         const std::vector<std::string>& roles)
    : participantID(participantID), name(name), roles(roles) {}

bool Participant::hasRole(const std::string& roleName) const {
    return std::find(roles.begin(), roles.end(), roleName) != roles.end();
}
