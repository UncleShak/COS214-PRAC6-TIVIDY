#include "AvailableWorkIterator.h"

#include <stdexcept>

AvailableWorkIterator::AvailableWorkIterator(const std::vector<std::string>& availableItems)
    : items(availableItems), position(0) {}

void AvailableWorkIterator::first() { position = 0; }

void AvailableWorkIterator::next() {
    if (!isDone()) ++position;
}

bool AvailableWorkIterator::isDone() const { return position >= items.size(); }

std::string AvailableWorkIterator::currentItem() const {
    if (isDone()) {
        throw std::out_of_range("AvailableWorkIterator: no current item, the traversal is done");
    }
    return items[position];
}
