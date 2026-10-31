#include "ActionAccessPolicy.h"
#include <stdexcept>

ActionAccessPolicy::ActionAccessPolicy() : RoleAccessPolicy(true) {}

void ActionAccessPolicy::restrict(const std::string& action, const std::vector<std::string>& roles) {
    if (action.empty() || roles.empty()) {
        throw std::invalid_argument("An access rule needs an action and at least one role");
    }
    restricted[action].insert(roles.begin(), roles.end());
}

bool ActionAccessPolicy::isRestricted(const std::string& action) const {
    return restricted.count(action) > 0;
}

std::vector<std::string> ActionAccessPolicy::rolesFor(const std::string& action) const {
    std::map<std::string, std::set<std::string> >::const_iterator found = restricted.find(action);
    if (found == restricted.end()) return std::vector<std::string>();
    return std::vector<std::string>(found->second.begin(), found->second.end());
}

bool ActionAccessPolicy::isAllowed(const std::string& actorRole,
                                   const std::string& requiredRole,
                                   const std::string& action) const {
    std::map<std::string, std::set<std::string> >::const_iterator found = restricted.find(action);
    if (found != restricted.end()) {
        return found->second.count(actorRole) > 0;
    }
    return RoleAccessPolicy::isAllowed(actorRole, requiredRole, action);
}
