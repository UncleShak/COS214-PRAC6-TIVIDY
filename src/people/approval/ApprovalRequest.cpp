#include "ApprovalRequest.h"
#include "ApprovalHandler.h"


#include <sstream>
#include <stdexcept>

ApprovalRequest::ApprovalRequest(int id, const std::string& itemName, const std::string& requesterName,
                    double value, const std::string& reason, int raisedAt) : id(id), itemName(itemName), requesterName(requesterName), value(value),
                    reason(reason), raisedAt(raisedAt), status(PENDING), handler(nullptr), deadline(NO_DEADLINE){}

void ApprovalRequest::assignTo(ApprovalHandler* handler, int deadline){

    if(status != PENDING){
        throw InvalidTransition("Approval Request #" + std::to_string(id) + " is already " + statusName(status) + "; cannot reassign");
    }
    
    this->handler = handler;
    this->deadline = deadline;
}

void ApprovalRequest::resolve(ApprovalStatus status, const std::string& decidedBy, const std::string& note){

    if(status == PENDING){

        throw std::invalid_argument("ApprovalRequest::resolve() must be called with APPROVED, REJECTED or EXPIRED, not PENDING");
    }

    if(this->status != PENDING){

        throw InvalidTransition("Approval Request #" + std::to_string(id) + " is already " + statusName(this->status));
    }

    this->status = status;
    this->decidedBy = decidedBy;
    this->decisionNote = note;
}


bool ApprovalRequest::isOverdue(int now) const{

    return status == PENDING && deadline != NO_DEADLINE && now >= deadline;

}


std::string ApprovalRequest::describe() const{

    std::ostringstream out;
    out << "#" << id << " '" << itemName << "' ("
        << reason << ", value ";

    
    if(value == static_cast<int>(value)){
        out << static_cast<int>(value);
    }else{
        
        out << value;
    }

    out << ") ";

    switch(status){
        case PENDING:
            out << "Pending";
            if(handler != nullptr) out << " at " << handler->getName();
            break;
        case APPROVED:
            out << "Approved by " << decidedBy;
            break;
        case REJECTED:
            out << "Rejected by " << decidedBy;
            break;
        case EXPIRED:
            out << "Expired";
            if(!decidedBy.empty()) out << " (" << decidedBy << ")";
            break;
    }
    return out.str();


}