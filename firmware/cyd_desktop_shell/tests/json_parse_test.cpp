// Regression tests for the request parser (issues D4 and D5).
//
// D5 was observed on the device: with the old strstr lookup,
//     {"id":"n1","command":"desktop_ui_tree","meta":{"limit":1},"limit":4}
// returned one UI node instead of four, because "limit" was found inside the
// nested `meta` object first.
//
// D4 was observed too: an id of 64 characters or more came back truncated to
// 63, so the client could never pair the reply with its request.

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "json_parse.h"

namespace json = cyd::desktop::json;

namespace {

long long integer_or(const char *line, const char *key, long long fallback) {
    long long value = fallback;
    json::integer_value(line, key, &value);
    return value;
}

void a_nested_key_does_not_shadow_the_top_level_one() {
    const char *line =
        "{\"id\":\"n1\",\"command\":\"desktop_ui_tree\",\"meta\":{\"limit\":1},\"limit\":4}";
    assert(integer_or(line, "limit", -1) == 4);

    // Also when the nested object comes after the real key.
    const char *reversed =
        "{\"limit\":4,\"meta\":{\"limit\":1},\"command\":\"desktop_ui_tree\"}";
    assert(integer_or(reversed, "limit", -1) == 4);

    // And when the only occurrence is nested, the key is simply absent.
    const char *nested_only = "{\"command\":\"x\",\"meta\":{\"limit\":9}}";
    assert(integer_or(nested_only, "limit", -1) == -1);
    assert(!json::has_key(nested_only, "limit"));
}

void a_key_inside_an_array_does_not_match() {
    const char *line = "{\"command\":\"x\",\"steps\":[{\"limit\":7}],\"limit\":2}";
    assert(integer_or(line, "limit", -1) == 2);
}

void a_key_spelled_inside_a_string_value_does_not_match() {
    // Confirmed on the device: escaping already prevented this, and the
    // structure-aware lookup must not regress it.
    const char *line = "{\"id\":\"\\\"limit\\\": 1 \",\"command\":\"x\",\"limit\":4}";
    assert(integer_or(line, "limit", -1) == 4);
}

void a_value_that_does_not_fit_is_refused_rather_than_truncated() {
    const char *line = "{\"id\":\"0123456789\",\"command\":\"x\"}";
    char small[5] = "junk";
    assert(!json::string_value(line, "id", small, sizeof(small)));
    assert(small[0] == '\0');  // never hand back a partial id

    char big[32] = "";
    assert(json::string_value(line, "id", big, sizeof(big)));
    assert(std::strcmp(big, "0123456789") == 0);
}

void an_absent_key_is_distinguishable_from_one_that_does_not_fit() {
    const char *line = "{\"id\":\"0123456789\",\"command\":\"x\"}";
    char small[5] = "";
    assert(!json::string_value(line, "id", small, sizeof(small)));
    assert(json::has_key(line, "id"));       // present, just too long
    assert(!json::has_key(line, "missing"));  // genuinely absent
}

void escapes_and_whitespace_are_handled() {
    const char *line = "{ \"title\" : \"a\\\"b\" , \"limit\" : 12 }";
    char title[16] = "";
    assert(json::string_value(line, "title", title, sizeof(title)));
    assert(std::strcmp(title, "a\"b") == 0);
    assert(integer_or(line, "limit", -1) == 12);
}

void malformed_input_is_refused_without_reading_past_the_end() {
    char scratch[16] = "";
    assert(!json::string_value("", "id", scratch, sizeof(scratch)));
    assert(!json::string_value("not json", "id", scratch, sizeof(scratch)));
    assert(!json::string_value("{\"id\":", "id", scratch, sizeof(scratch)));
    assert(!json::string_value("{\"id\":\"unterminated", "id", scratch, sizeof(scratch)));
    assert(!json::string_value("{\"id\":5}", "id", scratch, sizeof(scratch)));  // not a string
    assert(integer_or("{\"limit\":\"4\"}", "limit", -1) == -1);                 // not a number
    assert(!json::has_key("{}", "id"));
}

void negative_and_large_integers_round_trip() {
    assert(integer_or("{\"offset\":-1}", "offset", 0) == -1);
    assert(integer_or("{\"limit\":99999}", "limit", 0) == 99999);
    assert(integer_or("{\"epoch\":1788711405}", "epoch", 0) == 1788711405LL);
}

void leading_whitespace_before_the_object_is_accepted() {
    // The reader now trims this before dispatching, so the parser must cope.
    assert(integer_or("  {\"limit\":3}", "limit", -1) == 3);
}

}  // namespace

int main() {
    a_nested_key_does_not_shadow_the_top_level_one();
    a_key_inside_an_array_does_not_match();
    a_key_spelled_inside_a_string_value_does_not_match();
    a_value_that_does_not_fit_is_refused_rather_than_truncated();
    an_absent_key_is_distinguishable_from_one_that_does_not_fit();
    escapes_and_whitespace_are_handled();
    malformed_input_is_refused_without_reading_past_the_end();
    negative_and_large_integers_round_trip();
    leading_whitespace_before_the_object_is_accepted();
    std::printf("json_parse_test: all assertions passed\n");
    return 0;
}
