#ifndef PARTICIPANT_H
#define PARTICIPANT_H
#include <string>
#include <vector>
#include "Role.h"

// A person or system that can do work. Holds its roles and its current workload.
class Participant {
private:
    std::string name;
    std::string participantID;
    std::vector<Role> roles;
    int workload;
public:
    Participant(const std::string& name, const std::string& id);
    void addRole(const Role& role);
    bool hasRole(const std::string& roleName) const;
    std::vector<std::string> getRoleNames() const;
    std::string getName() const;
    std::string getID() const;
    int getWorkload() const;
    void workAssigned();
    void workFinished();
};
#endif
