/**
 * @file RoleAccessPolicy.cpp
 * @brief Implementation of RoleAccessPolicy.
 *
 * @author Sloth (The Archivist)
 */
#include "RoleAccessPolicy.h"

RoleAccessPolicy::RoleAccessPolicy(bool allowAdminOverride)
    : allowAdminOverride(allowAdminOverride) {
}

bool RoleAccessPolicy::isAllowed(const std::string& actorRole,
                                 const std::string& requiredRole,
                                 const std::string& /*action*/) const {
    // 1. If no specific role is required, any participant can execute
    if (requiredRole.empty()) {
        return true;
    }

    // 2. Direct role match
    if (actorRole == requiredRole) {
        return true;
    }

    // 3. Administrative override if enabled
    if (allowAdminOverride) {
        if (actorRole == "Administrator" || actorRole == "Admin" || actorRole == "Manager") {
            return true;
        }
    }

    return false;
}
