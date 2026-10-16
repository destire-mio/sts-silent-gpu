#pragma once
#include "schema.hpp"

namespace spire::obs {
SPIRE_FN inline uint64_t stored_bytes(int entities, int candidates) {
    return 4 + entity_bytes * entities + candidate_bytes * candidates;
}
SPIRE_FN inline void put_word(uint8_t*& output, int value) {
    const uint16_t bits = static_cast<uint16_t>(value);
    *output++ = bits & 255;
    *output++ = bits >> 8;
}
SPIRE_FN inline int get_word(const uint8_t*& input) {
    const int bits = input[0] | (static_cast<int>(input[1]) << 8);
    input += 2;
    return bits >= 32768 ? bits - 65536 : bits;
}
SPIRE_FN inline void store(View view, uint8_t* output) {
    const int entities = *view.entity_count, candidates = *view.candidate_count;
    put_word(output, entities); put_word(output, candidates);
    for (int row = 0; row < tag_rows; ++row)
        for (int i = 0; i < entities; ++i) *output++ = view.tags[row * entity_capacity + i];
    for (int row = 0; row < id_rows; ++row)
        for (int i = 0; i < entities; ++i) put_word(output, view.ids[row * entity_capacity + i]);
    for (int row = 0; row < value_rows; ++row)
        for (int i = 0; i < entities; ++i) put_word(output, view.values[row * entity_capacity + i]);
    for (int i = 0; i < candidates; ++i) *output++ = view.candidate_kinds[i];
    for (int row = 0; row < candidate_rows; ++row)
        for (int i = 0; i < candidates; ++i) put_word(output, view.candidate_refs[row * candidate_capacity + i]);
}
SPIRE_FN inline void pad(const uint8_t* input, View view) {
    const int entities = get_word(input), candidates = get_word(input);
    for (int row = 0; row < tag_rows; ++row)
        for (int i = 0; i < entities; ++i) view.tags[row * entity_capacity + i] = *input++;
    for (int row = 0; row < id_rows; ++row)
        for (int i = 0; i < entities; ++i) view.ids[row * entity_capacity + i] = get_word(input);
    for (int row = 0; row < value_rows; ++row)
        for (int i = 0; i < entities; ++i) view.values[row * entity_capacity + i] = get_word(input);
    for (int i = 0; i < candidates; ++i) view.candidate_kinds[i] = *input++;
    for (int row = 0; row < candidate_rows; ++row)
        for (int i = 0; i < candidates; ++i) view.candidate_refs[row * candidate_capacity + i] = get_word(input);
    view.finish(entities, candidates);
}
}
