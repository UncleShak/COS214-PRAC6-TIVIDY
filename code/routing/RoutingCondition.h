#ifndef ROUTING_CONDITION_H
#define ROUTING_CONDITION_H

#include <functional>
#include <string>

#include "DataPayload.h"

class RoutingCondition {
public:
    typedef std::function<bool(const DataPayload&)> Predicate;
    RoutingCondition(const std::string& label, const Predicate& predicate);
    static RoutingCondition equals(const std::string& field, const std::string& value);
    static RoutingCondition notEquals(const std::string& field, const std::string& value);
    static RoutingCondition greaterThan(const std::string& field, double limit);
    static RoutingCondition atMost(const std::string& field, double limit);

    /// Data field where WorkflowInstance records how an item finished: "<item>.outcome",
    /// set to "completed" or "rejected". It lives in the instance data, so undo restores it.
    static std::string outcomeField(const std::string& item);
    /// True when `item` finished by being rejected. Put this branch first at a decision point:
    /// the first matching branch wins, and the other branches read data a rejection never changed.
    static RoutingCondition rejected(const std::string& item);
    bool evaluate(const DataPayload& data) const;
    const std::string& getLabel() const;

private:
    std::string label;
    Predicate predicate;
};

#endif
