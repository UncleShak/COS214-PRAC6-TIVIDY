/**
 * @file test_validation.cpp
 * @brief Free-text and menu input checks used by both front ends.
 * @author Shakir Alexander (Lust)
 */
#include "doctest.h"

#include <string>

#include "InputValidation.h"

using namespace tividy::ui;

TEST_CASE("trim removes surrounding whitespace only") {
    CHECK(trim("  Gotham  ") == "Gotham");
    CHECK(trim("\tnew york\n") == "new york");
    CHECK(trim("   ") == "");
    CHECK(trim("") == "");
}

TEST_CASE("isNumber follows KeyValuePayload::getNumber: the whole string must parse") {
    CHECK(isNumber("1500"));
    CHECK(isNumber(" 12.5 "));
    CHECK(isNumber("-3"));
    CHECK_FALSE(isNumber(""));
    CHECK_FALSE(isNumber("   "));
    CHECK_FALSE(isNumber("12abc"));
    CHECK_FALSE(isNumber("abc"));
    CHECK_FALSE(isNumber("1 000"));
}

TEST_CASE("parseYesNo accepts common spellings in any case") {
    bool value = false;
    CHECK(parseYesNo("YES", value));
    CHECK(value);
    CHECK(parseYesNo(" n ", value));
    CHECK_FALSE(value);
    CHECK_FALSE(parseYesNo("maybe", value));
    CHECK_FALSE(parseYesNo("", value));
}

TEST_CASE("parseMenuChoice takes a 1-based number within range") {
    std::size_t index = 99;
    CHECK(parseMenuChoice("1", 3, index));
    CHECK(index == 0);
    CHECK(parseMenuChoice(" 3 ", 3, index));
    CHECK(index == 2);

    CHECK_FALSE(parseMenuChoice("0", 3, index));
    CHECK_FALSE(parseMenuChoice("4", 3, index));
    CHECK_FALSE(parseMenuChoice("", 3, index));
    CHECK_FALSE(parseMenuChoice("two", 3, index));
    CHECK_FALSE(parseMenuChoice("-1", 3, index));
    CHECK_FALSE(parseMenuChoice("1.5", 3, index));
    CHECK_FALSE(parseMenuChoice("99999999999", 3, index));
    CHECK_FALSE(parseMenuChoice("1", 0, index));
}

TEST_CASE("validateField applies required, numeric, length and default rules") {
    DataField amount = {"amount", "Order amount", true, true, ""};
    DataField urgent = {"urgent", "Urgent", false, false, "no"};
    std::string cleaned;

    CHECK(validateField(amount, " 1500 ", cleaned) == "");
    CHECK(cleaned == "1500");

    CHECK(validateField(amount, "", cleaned) == "Order amount is required");
    CHECK(validateField(amount, "lots", cleaned) == "Order amount must be a number");

    CHECK(validateField(urgent, "  ", cleaned) == "");
    CHECK(cleaned == "no");

    std::string tooLong(kMaxTextLength + 1, 'x');
    CHECK(validateField(urgent, tooLong, cleaned) != "");
}
