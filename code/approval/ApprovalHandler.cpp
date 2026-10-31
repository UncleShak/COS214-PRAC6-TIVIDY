#include "ApprovalHandler.h"
#include "ApprovalRequest.h"
#include "Participant.h"

#include <stdexcept>
#include <string>

ApprovalHandler::ApprovalHandler(const std::string& levelName) : levelName(levelName), successor(nullptr){}

ApprovalHandler::~ApprovalHandler(){

    delete successor;
}

ApprovalHandler* ApprovalHandler::setNext(ApprovalHandler* next){
    
    if(next == this){
        throw std::invalid_argument("ApprovalHandler::setNext: a handler cannot be its own successor");        
    }

    if(next != successor){
        delete successor;
    }

    successor = next;

    return next;
}

void ApprovalHandler::handleRequest(ApprovalRequest* request, int now){

    if(request == nullptr){

        throw std::invalid_argument("ApprovalHandler::handleRequest: request is null");
    }

    if(successor){
        successor->handleRequest(request, now);
        return;
    }else{

        request->resolve(REJECTED, "(chain)", "no approver has sufficient authority");
    }
}

void ApprovalHandler::escalate(ApprovalRequest* request, int now){

    if(request == nullptr){

        throw std::invalid_argument("ApprovalHandler::escalate: request is null");
    }

    if(successor){
        successor->handleRequest(request, now);
        return;
    }else{

        request->resolve(EXPIRED, "(chain)", "escalation exhausted: nobody responded");

    }
}

bool ApprovalHandler::mayDecide(const Participant& actor) const{

    const std::string role = getRole();
    
    return !role.empty() && actor.hasRole(role);
}