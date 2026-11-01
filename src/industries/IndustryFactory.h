#ifndef TIVIDY_INDUSTRY_FACTORY_H
#define TIVIDY_INDUSTRY_FACTORY_H

#include <memory>
#include <string>
#include <vector>
#include "WorkflowBuilder.h"
#include "DataPayload.h"
#include "Participant.h"

/**
 * @brief Abstract Factory: one industry's family of related products.
 *
 * Each ConcreteFactory creates the products that belong together for its industry:
 *  - a WorkflowBuilder (given to the EngineConfigurator Director) for its process,
 *  - the participants and roles who work in it,
 *  - realistic starting data for one new instance.
 * The engine and the interface only see this interface, so a new industry is one new
 * ConcreteFactory and its builder, with no change to the engine.
 */
class IndustryFactory {
public:
    virtual ~IndustryFactory() {}

    /** @brief Engine name of the workflow, equal to the builder's root stage name. */
    virtual std::string getWorkflowName() const = 0;
    virtual std::string getDisplayName() const = 0;
    virtual std::string getDescription() const = 0;
    /** @brief Sector shown beside the name, e.g. "Logistics". Empty if not given. */
    virtual std::string getSector() const { return ""; }
    /** @brief Short prefix for instance ids, e.g. "AX" gives AX-1, AX-2. */
    virtual std::string getIdPrefix() const = 0;

    virtual std::unique_ptr<WorkflowBuilder> createBuilder() const = 0;
    virtual std::vector<std::unique_ptr<Participant> > createParticipants() const = 0;
    /** @param instanceId used where the data names the case (order id, run id). */
    virtual std::unique_ptr<DataPayload> createInitialData(const std::string& instanceId) const = 0;
    /** @brief Domain events this industry reacts to (interrupt event types), for the interface. */
    virtual std::vector<std::string> getEventTypes() const = 0;
};

#endif
