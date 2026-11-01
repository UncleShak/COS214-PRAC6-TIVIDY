#include "CommandLog.h"

#include "Command.h"

#include <stdexcept>

void CommandLog::run(Command* command) {
    if (command == nullptr) {
        throw std::invalid_argument("CommandLog cannot run a null command");
    }
    std::unique_ptr<Command> owned(command);  // Deleted automatically if execute() throws.
    owned->execute();
    done.push_back(std::move(owned));
}

std::vector<std::string> CommandLog::describeAll() const {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < done.size(); ++i) {
        out.push_back(done[i]->describe());
    }
    return out;
}

void CommandLog::truncate(std::size_t count) {
    if (count > done.size()) throw std::invalid_argument("Invalid saved command count");
    while (done.size() > count) done.pop_back();
}