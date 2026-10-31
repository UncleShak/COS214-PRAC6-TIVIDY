/**
 * @file AccessControlProxy.h
 * @brief Protection Proxy guarding execution of a WorkflowComponent.
 *
 * Pattern: Proxy (Protection Proxy role).
 * GoF Participants:
 * - Subject: WorkflowComponent
 * - RealSubject: WorkItem, Stage
 * - Proxy: AccessControlProxy
 *
 * Guards calls to request(), add(), and remove() by verifying participant
 * roles against an AccessPolicy strategy. If permission is denied, execution
 * is halted and an exception is thrown, preventing unauthorized state changes.
 *
 * Distinct from Decorator (which augments behavior such as auditing or retries):
 * the Proxy's primary responsibility is access gatekeeping and refusal.
 *
 * @author Sloth (The Archivist)
 */
#ifndef TIVIDY_ACCESS_CONTROL_PROXY_H
#define TIVIDY_ACCESS_CONTROL_PROXY_H

#include "WorkflowComponent.h"
#include "AccessPolicy.h"
#include <string>

class AccessControlProxy : public WorkflowComponent {
public:
    /**
     * @brief Constructs an AccessControlProxy around a real subject.
     * @param realSubject The wrapped component (WorkItem or Stage).
     * @param policy The access check policy strategy.
     * @param actorRole The current participant's role.
     * @param requiredRole The role required to access this component.
     * @param ownsSubject If true, destructing the proxy will delete realSubject.
     */
    AccessControlProxy(WorkflowComponent* realSubject,
                       AccessPolicy* policy,
                       const std::string& actorRole = "",
                       const std::string& requiredRole = "",
                       bool ownsSubject = false);

    ~AccessControlProxy() override;

    std::string getName() const override;

    /**
     * @brief Guards execution of the wrapped component.
     * @throws std::logic_error if access is denied by the policy.
     */
    void request() override;

    /**
     * @brief Guards structural addition to the wrapped component.
     * @throws std::logic_error if access is denied.
     */
    void add(WorkflowComponent* component) override;

    /**
     * @brief Guards structural removal from the wrapped component.
     * @throws std::logic_error if access is denied.
     */
    void remove(WorkflowComponent* component) override;

    /**
     * @brief Passes inspection/reporting visitor to the real subject.
     */
    void accept(WorkflowVisitor* visitor) override;

    void setActorRole(const std::string& role);
    std::string getActorRole() const;

    void setRequiredRole(const std::string& role);
    std::string getRequiredRole() const;

    void setPolicy(AccessPolicy* newPolicy);
    AccessPolicy* getPolicy() const;

    WorkflowComponent* getRealSubject() const;

private:
    void checkAccess(const std::string& action) const;

    WorkflowComponent* realSubject;
    AccessPolicy* policy;
    std::string actorRole;
    std::string requiredRole;
    bool ownsSubject;
};

#endif // TIVIDY_ACCESS_CONTROL_PROXY_H
