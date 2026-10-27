#include "Role.h"

Role::Role(const std::string& name) : roleName(name) {}

std::string Role::getName() const { 
    return roleName; 
}

bool Role::matches(const std::string& otherRole) const {
    return roleName == otherRole;
}
