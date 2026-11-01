/**
 * @file RealEngineFactory.cpp
 * @brief makeEngine() for the shipped program: the real engine with every industry installed.
 */
#include "EngineFactory.h"
#include "RealEngine.h"

namespace tividy {
namespace ui {

std::unique_ptr<EngineGateway> makeEngine() {
    return std::unique_ptr<EngineGateway>(new RealEngine());
}

const char* engineLabel() {
    return "TIVIDY engine";
}

} // namespace ui
} // namespace tividy
