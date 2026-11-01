#include "IndustryCatalog.h"
#include "ArkhamExpressFactory.h"
#include "DuneHarvestFactory.h"

std::vector<std::unique_ptr<IndustryFactory> > createAllIndustries() {
    std::vector<std::unique_ptr<IndustryFactory> > all;
    all.push_back(std::unique_ptr<IndustryFactory>(new ArkhamExpressFactory()));
    all.push_back(std::unique_ptr<IndustryFactory>(new DuneHarvestFactory()));
    return all;
}
