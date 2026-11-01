#ifndef TIVIDY_CONNECTOR_H
#define TIVIDY_CONNECTOR_H
#include <memory>
#include <string>
#include "DataPayload.h"
/** @brief Adapter Target: how the engine calls an external system for one work item.
 * invoke never changes its input; it returns a new payload holding the response.
 * Business refusals are data in the response; transport failures throw std::runtime_error.
 */
class Connector {
public:
    virtual ~Connector() = default;
    virtual bool isAvailable() const = 0;
    virtual std::unique_ptr<DataPayload> invoke(const std::string& operation, const DataPayload& data) = 0;
    // Optional compensation hooks. State is private to the Adapter, not editable case data.
    // Stateless connectors keep the defaults. Snapshots own the returned state.
    virtual std::unique_ptr<DataPayload> saveExternalState(const DataPayload&) const { return nullptr; }
    virtual void restoreExternalState(const DataPayload&, const DataPayload&) {}
    virtual std::unique_ptr<DataPayload> cancelExternal(const DataPayload&) { return nullptr; }
    
};
#endif
