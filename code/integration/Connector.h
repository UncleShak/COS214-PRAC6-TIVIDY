#ifndef TIVIDY_CONNECTOR_H
#define TIVIDY_CONNECTOR_H
#include <memory>
#include <string>
#include "../DataPayload.h"
/** @brief Adapter Target: how the engine calls an external system for one work item.
 * invoke never changes its input; it returns a new payload holding the response.
 * Business refusals are data in the response; transport failures throw std::runtime_error.
 */
class Connector {
public:
    virtual ~Connector() = default;
    virtual bool isAvailable() const = 0;
    virtual std::unique_ptr<DataPayload> invoke(const std::string& operation, const DataPayload& data) = 0;
};
#endif
