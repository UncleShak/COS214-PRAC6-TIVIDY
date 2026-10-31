#ifndef TIVIDY_ACTION_ACCESS_POLICY_H
#define TIVIDY_ACTION_ACCESS_POLICY_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include "RoleAccessPolicy.h"

/**
 * @brief Per-organisation access rules, supplied by the WorkflowDefinition (Proxy's policy).
 *
 * An action can be restricted to a set of roles, for example "only an OperationsManager may
 * cancel". A restricted action is allowed only for those roles. Every other action follows
 * RoleAccessPolicy: the actor needs the work item's required role, or an admin/manager role.
 */
class ActionAccessPolicy : public RoleAccessPolicy {
public:
    ActionAccessPolicy();

    /** @throws std::invalid_argument for an empty action or role list. */
    void restrict(const std::string& action, const std::vector<std::string>& roles);
    bool isRestricted(const std::string& action) const;
    std::vector<std::string> rolesFor(const std::string& action) const;

    bool isAllowed(const std::string& actorRole,
                   const std::string& requiredRole,
                   const std::string& action) const override;

private:
    std::map<std::string, std::set<std::string> > restricted;
};

#endif
