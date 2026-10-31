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
    bool evaluate(const DataPayload& data) const;
    const std::string& getLabel() const;

private:
    std::string label;
    Predicate predicate;
};

#endif
