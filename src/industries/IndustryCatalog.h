#ifndef TIVIDY_INDUSTRY_CATALOG_H
#define TIVIDY_INDUSTRY_CATALOG_H

#include <memory>
#include <vector>
#include "IndustryFactory.h"

/** @brief Every industry this build ships with, one ConcreteFactory each. */
std::vector<std::unique_ptr<IndustryFactory> > createAllIndustries();

#endif
