/**
 * @file FakeEngineFactory.cpp
 * @brief makeEngine() for the development-only `tividy_dev` program. Never shipped.
 * @author Shakir Alexander (Lust)
 */
#include "EngineFactory.h"
#include "FakeEngine.h"

namespace tividy {
namespace ui {

std::unique_ptr<EngineGateway> makeEngine() {
    return std::unique_ptr<EngineGateway>(new FakeEngine());
}

const char* engineLabel() {
    return "FAKE ENGINE - development only";
}

} // namespace ui
} // namespace tividy
