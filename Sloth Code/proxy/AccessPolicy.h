/**
 * @file AccessPolicy.h
 * @brief Strategy interface for access authorization rules used by AccessControlProxy.
 *
 * Pattern: Proxy helper / Strategy.
 * Decides whether a given participant or role is permitted to perform
 * an operation on a WorkflowComponent.
 *
 * @author Sloth (The Archivist)
 */
#ifndef TIVIDY_ACCESS_POLICY_H
#define TIVIDY_ACCESS_POLICY_H

#include <string>

class AccessPolicy {
public:
    virtual ~AccessPolicy() = default;

    /**
     * @brief Checks if an actor with the given role can perform the action.
     * @param actorRole The role of the participant initiating the request.
     * @param requiredRole The role required by the work item.
     * @param action The operation being attempted (e.g. "request", "add", "remove").
     * @return true if access is granted, false if forbidden.
     */
    virtual bool isAllowed(const std::string& actorRole,
                           const std::string& requiredRole,
                           const std::string& action) const = 0;
};

#endif // TIVIDY_ACCESS_POLICY_H
