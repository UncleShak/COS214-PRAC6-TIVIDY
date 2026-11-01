#include "RoutingCondition.h"

#include "KeyValuePayload.h"

#include <sstream>
#include <stdexcept>

namespace {
std::string numberText(double value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

// Built-in conditions read named fields, which KeyValuePayload provides.
// Any other payload type has no named fields, so these conditions are false
// for it and the decision falls through to its default branch. Industries with
// their own payload type can still pass a custom Predicate.
const KeyValuePayload* fieldsOf(const DataPayload& data) {
    return dynamic_cast<const KeyValuePayload*>(&data);
}

bool readText(const DataPayload& data, const std::string& field, std::string& out) {
    const KeyValuePayload* fields = fieldsOf(data);
    if (fields == nullptr || !fields->has(field)) return false;
    out = fields->get(field);
    return true;
}

bool readNumber(const DataPayload& data, const std::string& field, double& out) {
    const KeyValuePayload* fields = fieldsOf(data);
    if (fields == nullptr || !fields->has(field)) return false;
    try {
        out = fields->getNumber(field);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}
}  // namespace

RoutingCondition::RoutingCondition(const std::string& conditionLabel, const Predicate& test)
    : label(conditionLabel), predicate(test) {
    if (!predicate) {
        throw std::invalid_argument("RoutingCondition: a condition needs a predicate");
    }
}

RoutingCondition RoutingCondition::equals(const std::string& field, const std::string& value) {
    return RoutingCondition("[" + field + " == " + value + "]",
        [field, value](const DataPayload& data) {
            std::string text;
            return readText(data, field, text) && text == value;
        });
}

std::string RoutingCondition::outcomeField(const std::string& item) {
    return item + ".outcome";
}

RoutingCondition RoutingCondition::rejected(const std::string& item) {
    return RoutingCondition("[" + item + " rejected]", equals(outcomeField(item), "rejected").predicate);
}

RoutingCondition RoutingCondition::notEquals(const std::string& field, const std::string& value) {
    return RoutingCondition("[" + field + " != " + value + "]",
        [field, value](const DataPayload& data) {
            std::string text;
            return readText(data, field, text) && text != value;
        });
}

RoutingCondition RoutingCondition::greaterThan(const std::string& field, double limit) {
    return RoutingCondition("[" + field + " > " + numberText(limit) + "]",
        [field, limit](const DataPayload& data) {
            double value = 0;
            return readNumber(data, field, value) && value > limit;
        });
}

RoutingCondition RoutingCondition::atMost(const std::string& field, double limit) {
    return RoutingCondition("[" + field + " <= " + numberText(limit) + "]",
        [field, limit](const DataPayload& data) {
            double value = 0;
            return readNumber(data, field, value) && value <= limit;
        });
}

bool RoutingCondition::evaluate(const DataPayload& data) const {
    return predicate(data);
}

const std::string& RoutingCondition::getLabel() const {
    return label;
}
