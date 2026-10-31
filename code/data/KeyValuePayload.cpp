#include "KeyValuePayload.h"

#include <cstdlib>
#include <stdexcept>

std::string KeyValuePayload::getPayloadType() const {
    return "KeyValue";
}

DataPayload* KeyValuePayload::clone() const {
    return new KeyValuePayload(*this);
}

void KeyValuePayload::set(const std::string& key, const std::string& value) {
    values[key] = value;
}

bool KeyValuePayload::has(const std::string& key) const {
    return values.find(key) != values.end();
}

std::string KeyValuePayload::get(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = values.find(key);
    if (it == values.end()) {
        throw std::out_of_range("KeyValuePayload: no value for '" + key + "'");
    }
    return it->second;
}

double KeyValuePayload::getNumber(const std::string& key) const {
    const std::string text = get(key);
    const char* begin = text.c_str();
    char* end = 0;
    const double number = std::strtod(begin, &end);

    if (end == begin || *end != '\0') {
        throw std::invalid_argument("KeyValuePayload: '" + key + "' is not a number ('" + text + "')");
    }
    return number;
}

const std::map<std::string, std::string>& KeyValuePayload::entries() const {
    return values;
}
