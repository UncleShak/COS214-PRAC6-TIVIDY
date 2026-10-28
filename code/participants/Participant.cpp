#include "Participant.h"

Participant::Participant(const std::string& n, const std::string& id)
    : name(n), participantID(id), workload(0) {}

void Participant::addRole(const Role& role) { 
    roles.push_back(role); 
}

bool Participant::hasRole(const std::string& roleName) const {
    for (int i = 0; i < roles.size(); i++) {
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
