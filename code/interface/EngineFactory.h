/**
 * @file EngineFactory.h
 * @brief Where main() gets its engine from.
 *
 * The definition is chosen at link time, not in main.cpp:
 * - the shipped `tividy` program links RealEngineFactory.cpp (the real engine);
 * - the development-only `tividy_dev` program links tests/lust/fake/FakeEngineFactory.cpp.
 *
 * The fake is never compiled into the shipped program.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_ENGINE_FACTORY_H
#define TIVIDY_ENGINE_FACTORY_H

#include <memory>

#include "EngineGateway.h"

namespace tividy {
namespace ui {

/** @brief Builds the engine the interface will drive. May throw if the engine cannot load. */
std::unique_ptr<EngineGateway> makeEngine();

/** @brief One-line name of the linked engine, shown in the header bar ("real" or "FAKE"). */
const char* engineLabel();

} // namespace ui
} // namespace tividy

#endif
