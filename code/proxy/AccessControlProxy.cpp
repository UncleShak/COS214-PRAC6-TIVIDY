/**
 * @file AccessControlProxy.cpp
 * @brief Implementation of AccessControlProxy.
 *
 * @author Sloth (The Archivist)
 */
#include "AccessControlProxy.h"
#include <stdexcept>

AccessControlProxy::AccessControlProxy(WorkflowComponent* realSubject, AccessPolicy* policy, const std::string& actorRole, const std::string& requiredRole, bool ownsSubject)
                                      : realSubject(realSubject), policy(policy), actorRole(actorRole), requiredRole(requiredRole), ownsSubject(ownsSubject) {
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
    if (policy != nullptr) {
        if (!policy->isAllowed(actorRole, requiredRole, action)) {
            throw std::logic_error("Access denied: role '" + actorRole +
                                   "' is not permitted to perform '" + action +
                                   "' on work item '" + getName() +
                                   "' (requires role '" + requiredRole + "')");
        }
    }
}

void AccessControlProxy::request() {
    checkAccess("request");
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

void AccessControlProxy::setPolicy(AccessPolicy* newPolicy) {
    policy = newPolicy;
}

AccessPolicy* AccessControlProxy::getPolicy() const {
    return policy;
}

WorkflowComponent* AccessControlProxy::getRealSubject() const {
    return realSubject;
}
