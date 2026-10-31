/**
 * @file RoleAccessPolicy.h
 * @brief Concrete AccessPolicy based on participant role matching.
 *
 * Implements role-based access control (RBAC). Allows access when:
 * 1. The work item requires no specific role (empty requiredRole).
 * 2. The actor's role directly matches requiredRole.
 * 3. The actor holds an administrative override role ("Administrator" or "Admin").
 *
 * @author Sloth (The Archivist)
 */
#ifndef TIVIDY_ROLE_ACCESS_POLICY_H
#define TIVIDY_ROLE_ACCESS_POLICY_H

#include "AccessPolicy.h"
#include <string>

class RoleAccessPolicy : public AccessPolicy {
public:
    RoleAccessPolicy(bool allowAdminOverride = true);
    ~RoleAccessPolicy() override = default;

    bool isAllowed(const std::string& actorRole,
                   const std::string& requiredRole,
                   const std::string& action) const override;

private:
    bool allowAdminOverride;
};

#endif // TIVIDY_ROLE_ACCESS_POLICY_H
