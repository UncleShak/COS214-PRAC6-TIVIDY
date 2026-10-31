#include "Participant.h"

Participant::Participant(const std::string& n, const std::string& id)
    : name(n), participantID(id), workload(0) {}

void Participant::addRole(const Role& role) { 
    roles.push_back(role); 
}

bool Participant::hasRole(const std::string& roleName) const {
    for (size_t i = 0; i < roles.size(); i++) {
        if (roles[i].matches(roleName)) 
        return true;
    }
    return false;
}

std::string Participant::getName() const { 
    return name; 
}

std::string Participant::getID() const { 
    return participantID; 
}

int Participant::getWorkload() const { 
    return workload; 
}

void Participant::workAssigned() { 
    workload++; 
}

void Participant::workFinished() {
    if (workload > 0) 
    workload--;
}

std::vector<std::string> Participant::getRoleNames() const {
    std::vector<std::string> names;
    for (size_t i = 0; i < roles.size(); i++) names.push_back(roles[i].getName());
    return names;
}
