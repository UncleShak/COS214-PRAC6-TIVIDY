/**
 * @file AccessControlProxy.cpp
 * @brief Implementation of AccessControlProxy.
 *
 * @author Sloth (The Archivist)
 */
#include "AccessControlProxy.h"
#include <stdexcept>
#include "Participant.h"
#include "WorkflowException.h"

AccessControlProxy::AccessControlProxy(WorkflowComponent* realSubject, const AccessPolicy* policy, const std::string& actorRole, const std::string& requiredRole, bool ownsSubject)
                                      : realSubject(realSubject), policy(policy), actorRole(actorRole), requiredRole(requiredRole), ownsSubject(ownsSubject) {
    if (realSubject == nullptr) {
        throw std::invalid_argument("AccessControlProxy requires a non-null realSubject");
    }
}

AccessControlProxy::AccessControlProxy(WorkflowComponent* subject, const AccessPolicy* accessPolicy,
                                       const Participant& actor, const std::string& required)
    : realSubject(subject), policy(accessPolicy), actorRoles(actor.getRoleNames()), actorName(actor.getName()),
      requiredRole(required), ownsSubject(false) {
    if (realSubject == nullptr) {
        throw std::invalid_argument("AccessControlProxy requires a non-null realSubject");
    }
}

AccessControlProxy::~AccessControlProxy() {
    if (ownsSubject) {
        delete realSubject;
    }
}

std::string AccessControlProxy::getName() const {
    return realSubject->getName();
}

void AccessControlProxy::checkAccess(const std::string& action) const {
    if (policy == nullptr) {
        return;
    }
    if (actorRoles.empty()) {
        if (!policy->isAllowed(actorRole, requiredRole, action)) {
            throw AccessDenied("Access denied: role '" + actorRole +
                               "' is not permitted to perform '" + action +
                               "' on work item '" + getName() +
                               "' (requires role '" + requiredRole + "')");
        }
        return;
    }
    for (std::size_t i = 0; i < actorRoles.size(); ++i) {
        if (policy->isAllowed(actorRoles[i], requiredRole, action)) {
            return;
        }
    }
    throw AccessDenied("Access denied: " + actorName + " may not " + action + " '" + getName() + "'");
}

void AccessControlProxy::authorize(const std::string& action) const {
    checkAccess(action);
    realSubject->authorize(action);   // a guarded component may itself be guarded
}

void AccessControlProxy::request() {
    // request() performs the work, which for a work item means completing it.
    checkAccess("complete");
    realSubject->request();
}

void AccessControlProxy::add(WorkflowComponent* component) {
    checkAccess("add");
    realSubject->add(component);
}

void AccessControlProxy::remove(WorkflowComponent* component) {
    checkAccess("remove");
    realSubject->remove(component);
}

void AccessControlProxy::accept(WorkflowVisitor* visitor) {
    // Visitor passes through so administrative audit and reports can still inspect structure
    realSubject->accept(visitor);
}

void AccessControlProxy::setActorRole(const std::string& role) {
    actorRole = role;
}

std::string AccessControlProxy::getActorRole() const {
    return actorRole;
}

void AccessControlProxy::setRequiredRole(const std::string& role) {
    requiredRole = role;
}

std::string AccessControlProxy::getRequiredRole() const {
    return requiredRole;
}

void AccessControlProxy::setPolicy(const AccessPolicy* newPolicy) {
    policy = newPolicy;
}

const AccessPolicy* AccessControlProxy::getPolicy() const {
    return policy;
}

WorkflowComponent* AccessControlProxy::getRealSubject() const {
    return realSubject;
}
