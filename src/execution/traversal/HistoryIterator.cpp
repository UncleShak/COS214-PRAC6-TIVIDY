#include "HistoryIterator.h"

#include <stdexcept>

HistoryIterator::HistoryIterator(const std::vector<std::string>& entries, Order order,
                                 const std::string& filter)
    : position(0) {
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const std::string& entry = (order == OldestFirst) ? entries[i] : entries[entries.size() - 1 - i];
        if (filter.empty() || entry.find(filter) != std::string::npos) {
            selected.push_back(entry);
        }
    }
}

void HistoryIterator::first() { position = 0; }

void HistoryIterator::next() {
    if (!isDone()) ++position;
}

bool HistoryIterator::isDone() const { return position >= selected.size(); }

std::string HistoryIterator::currentItem() const {
    if (isDone()) {
        throw std::out_of_range("HistoryIterator: no current item, the traversal is done");
    }
    return selected[position];
}
