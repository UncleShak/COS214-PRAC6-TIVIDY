#include "AutoApprover.h"
#include "ApprovalHandler.h"
#include "ApprovalRequest.h"

#include <string>
#include <stdexcept> 

AutoApprover::AutoApprover(const std::string& levelName, double maxValue) : ApprovalHandler(levelName), maxValue(maxValue) {}


void AutoApprover::handleRequest(ApprovalRequest* request, int now){

    if(request == nullptr){

        throw std::invalid_argument("AutoApprover::handleRequest: request is null");
    }

    if(withinLimit(request->getValue(), maxValue)){

        request->resolve(APPROVED, "auto:" + getName(), "within automatic limit");
    }else{
        
        ApprovalHandler::handleRequest(request, now);
    }

}