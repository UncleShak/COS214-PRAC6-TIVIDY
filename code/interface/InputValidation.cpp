#include "InputValidation.h"

#include <cctype>
#include <cstdlib>

namespace tividy {
namespace ui {

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool isNumber(const std::string& text) {
    std::string value = trim(text);
    if (value.empty()) {
        return false;
    }
    const char* start = value.c_str();
    char* stop = 0;
    std::strtod(start, &stop);
    return stop != start && *stop == '\0';
}

bool parseYesNo(const std::string& text, bool& value) {
    std::string lower;
    std::string trimmed = trim(text);
    for (std::size_t i = 0; i < trimmed.size(); ++i) {
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[i])));
    }
    if (lower == "y" || lower == "yes" || lower == "true") {
        value = true;
        return true;
    }
    if (lower == "n" || lower == "no" || lower == "false") {
        value = false;
        return true;
    }
    return false;
}

bool parseMenuChoice(const std::string& text, std::size_t count, std::size_t& index) {
    std::string value = trim(text);
    if (value.empty() || value.size() > 6) {
        return false;
    }
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    unsigned long choice = std::strtoul(value.c_str(), 0, 10);
    if (choice < 1 || choice > count) {
        return false;
    }
    index = static_cast<std::size_t>(choice - 1);
    return true;
}

std::string validateField(const DataField& field, const std::string& raw, std::string& cleaned) {
    std::string value = trim(raw);
    if (value.empty()) {
        if (field.required) {
            return field.label + " is required";
        }
        cleaned = field.defaultValue;
        return "";
    }
    if (value.size() > kMaxTextLength) {
        return field.label + " is too long (at most 200 characters)";
    }
    if (field.numeric && !isNumber(value)) {
        return field.label + " must be a number";
    }
    cleaned = value;
    return "";
}

} // namespace ui
} // namespace tividy
