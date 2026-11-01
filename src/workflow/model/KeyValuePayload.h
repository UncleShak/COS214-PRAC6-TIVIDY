#ifndef KEY_VALUE_PAYLOAD_H
#define KEY_VALUE_PAYLOAD_H

#include <map>
#include <string>

#include "DataPayload.h"

/**
 * @file KeyValuePayload.h
 * @brief Generic concrete DataPayload: an instance's data as named text fields.
 *
 * Industries put whatever their workflow needs in here ("amount", "inStock",
 * "verdict", ...) when an instance is created or as work items complete. The
 * routing conditions (RoutingCondition) read these fields to pick a branch, so
 * no industry-specific logic lives in the engine.
 *
 * Extends the team's abstract DataPayload, so a WorkflowInstance can hold it
 * through its DataPayload* and clone it for instance isolation and Memento
 * snapshots.
 */
class KeyValuePayload : public DataPayload {
public:
    KeyValuePayload() {}
    virtual ~KeyValuePayload() = default;

    /// @return "KeyValue"
    std::string getPayloadType() const override;
    /// Deep copy (Prototype / Memento support).
    DataPayload* clone() const override;

    /// Sets or overwrites a field.
    void set(const std::string& key, const std::string& value);
    /// @return true if the field has been set.
    bool has(const std::string& key) const;
    /// @throws std::out_of_range if the field is missing.
    std::string get(const std::string& key) const;
    /// @throws std::out_of_range if missing, std::invalid_argument if not numeric.
    double getNumber(const std::string& key) const;
    /// All fields, e.g. for reports.
    const std::map<std::string, std::string>& entries() const;

private:
    std::map<std::string, std::string> values;
};

#endif
