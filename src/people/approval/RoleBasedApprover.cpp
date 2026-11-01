#include "RoleBasedApprover.h"
#include "ApprovalRequest.h"

#include <string>
#include <stdexcept>

RoleBasedApprover::RoleBasedApprover(const std::string& levelName, const std::string& role, double maxValue, int timeLimit)
                    : ApprovalHandler(levelName), role(role), maxValue(maxValue), timeLimit(timeLimit){}



void RoleBasedApprover::handleRequest(ApprovalRequest* request, int now){

    if(request == nullptr){

        throw std::invalid_argument("RoleBasedApprover::handleRequest: request is null");
    }

    if(withinLimit(request->getValue(), maxValue)){

        int deadline = (timeLimit == 0) ? NO_DEADLINE : (now + timeLimit);
        request->assignTo(this, deadline);
        
    }else{

        ApprovalHandler::handleRequest(request, now);
    }
}

std::string RoleBasedApprover::getRole() const{
    return role;
}