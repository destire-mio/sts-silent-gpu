#pragma once
#include "core/engine.hpp"

namespace spire::obs {
constexpr int entity_capacity = 384;
constexpr int candidate_capacity = 128;
constexpr int tag_rows = 4;
constexpr int id_rows = 4;
constexpr int value_rows = 6;
constexpr int candidate_rows = 3;
constexpr int entity_mask_bytes = entity_capacity / 8;
constexpr int candidate_mask_bytes = candidate_capacity / 8;
constexpr int entity_bytes = tag_rows + 2 * (id_rows + value_rows);
constexpr int candidate_bytes = 1 + 2 * candidate_rows;
enum class Kind : int { Screen = 1, Player, Card, Monster, Power, Relic, Potion, Orb, Stance, MapNode, Option, Reward };
enum class Zone : int { None, Master, Hand, Draw, Discard, Exhaust, Selected, Grid, Offer, Shop, Event, History };
enum class Fault : uint32_t { None, Entities = 0xe0000001, Candidates, NoDecision, InvalidCandidate, SeedsExhausted, NumericRange };
enum class Control : int { Bowl = 1, Purge, OpenChest, Merchant, Campfire, EventButton, Spin, CardBack, Boss };

struct Token {
    int32_t ids[6];
    int32_t values[14];
};
struct View {
    uint8_t* tags;
    int16_t* ids;
    int16_t* values;
    uint8_t* candidate_kinds;
    int16_t* candidate_refs;
    uint8_t* entity_mask;
    uint8_t* candidate_mask;
    int32_t* entity_count;
    int32_t* candidate_count;

    SPIRE_FN void finish(int entities, int candidates, int written_entities = 0, int written_candidates = 0) const {
        const int used_entities = *entity_count > written_entities ? *entity_count : written_entities;
        const int used_candidates = *candidate_count > written_candidates ? *candidate_count : written_candidates;
        for (int row = 0; row < tag_rows; ++row)
            for (int i = entities; i < used_entities; ++i) tags[row * entity_capacity + i] = 0;
        for (int row = 0; row < id_rows; ++row)
            for (int i = entities; i < used_entities; ++i) ids[row * entity_capacity + i] = 0;
        for (int row = 0; row < value_rows; ++row)
            for (int i = entities; i < used_entities; ++i) values[row * entity_capacity + i] = 0;
        for (int i = candidates; i < used_candidates; ++i) candidate_kinds[i] = 0;
        for (int row = 0; row < candidate_rows; ++row)
            for (int i = candidates; i < used_candidates; ++i) candidate_refs[row * candidate_capacity + i] = 0;
        const int mask_entities = entities > used_entities ? entities : used_entities;
        const int mask_candidates = candidates > used_candidates ? candidates : used_candidates;
        for (int i = 0; i < (mask_entities + 7) / 8; ++i)
            entity_mask[i] = entities >= (i + 1) * 8 ? 255 : entities <= i * 8 ? 0 : (1u << (entities - i * 8)) - 1;
        for (int i = 0; i < (mask_candidates + 7) / 8; ++i)
            candidate_mask[i] = candidates >= (i + 1) * 8 ? 255 : candidates <= i * 8 ? 0 : (1u << (candidates - i * 8)) - 1;
        *entity_count = entities; *candidate_count = candidates;
    }
};
struct Observation {
    uint8_t tags[tag_rows * entity_capacity]{};
    int16_t ids[id_rows * entity_capacity]{};
    int16_t values[value_rows * entity_capacity]{};
    uint8_t candidate_kinds[candidate_capacity]{};
    int16_t candidate_refs[candidate_rows * candidate_capacity]{};
    uint8_t entity_mask[entity_mask_bytes]{};
    uint8_t candidate_mask[candidate_mask_bytes]{};
    int32_t entities = 0, candidates = 0;
    SPIRE_FN View view() { return {tags, ids, values, candidate_kinds, candidate_refs, entity_mask, candidate_mask, &entities, &candidates}; }
};
struct Result { uint32_t fault = 0; int entities = 0; int candidates = 0; };
struct Writer {
    View view;
    Result result{};
    SPIRE_FN uint8_t byte(int value) {
        if (value < 0 || value > 255) { result.fault = static_cast<uint32_t>(Fault::NumericRange); return 0; }
        return static_cast<uint8_t>(value);
    }
    SPIRE_FN int16_t word(int value) {
        if (value < -32768 || value > 32767) { result.fault = static_cast<uint32_t>(Fault::NumericRange); return 0; }
        return static_cast<int16_t>(value);
    }
    SPIRE_FN int bit(int value, int shift) {
        if (value < 0 || value > 1) { result.fault = static_cast<uint32_t>(Fault::NumericRange); return 0; }
        return value << shift;
    }
    SPIRE_FN int add(const Token& token) {
        if (result.fault) return 0;
        if (result.entities == entity_capacity) {
            result.fault = static_cast<uint32_t>(Fault::Entities);
            return 0;
        }
        const int index = result.entities++;
        const auto* v = token.values;
        int flags = 0, auxiliary = 0;
        int values[value_rows]{};
        switch (static_cast<Kind>(token.ids[0])) {
        case Kind::Screen:
            for (int i = 0; i < 3; ++i) { values[i] = v[i]; flags |= bit(v[3 + i], i); }
            values[3] = v[6]; values[4] = v[8]; values[5] = v[9]; auxiliary = v[7];
            flags |= bit(v[10], 3) | bit(v[11], 4) | bit(v[12], 5);
            break;
        case Kind::Card:
            values[0] = v[0]; values[1] = v[1]; values[2] = v[3]; values[3] = v[4]; values[4] = v[5]; values[5] = v[11];
            if (v[2] < 0 || v[2] > 7) result.fault = static_cast<uint32_t>(Fault::NumericRange);
            else flags = v[2];
            flags |= bit(v[6], 3) | bit(v[7], 4) | bit(v[8], 5) | bit(v[12], 6) | bit(v[13], 7);
            auxiliary = v[10];
            break;
        case Kind::Monster:
            for (int i = 0; i < 4; ++i) values[i] = v[i];
            values[4] = v[7]; values[5] = v[8]; flags = bit(v[4], 0); auxiliary = v[6];
            break;
        case Kind::Power:
            flags = bit(v[0], 0); values[0] = v[1]; values[1] = v[2]; values[2] = v[3];
            break;
        case Kind::Relic:
            flags = bit(v[0], 0) | bit(v[2], 1) | bit(v[3], 2); values[0] = v[1]; values[1] = v[11];
            break;
        case Kind::Potion: values[0] = v[0]; values[1] = v[11]; break;
        case Kind::MapNode:
            for (int i = 0; i < 3; ++i) { values[i] = v[i]; flags |= bit(v[3 + i], i); }
            break;
        case Kind::Option:
            for (int i = 0; i < 4; ++i) values[i] = v[i];
            values[4] = v[9]; values[5] = v[10];
            break;
        case Kind::Player: for (int i = 0; i < 5; ++i) values[i] = v[i]; break;
        case Kind::Orb: case Kind::Reward: values[0] = v[0]; values[1] = v[1]; break;
        case Kind::Stance: break;
        }
        view.tags[index] = byte(token.ids[0]);
        view.tags[entity_capacity + index] = byte(token.ids[2]);
        view.tags[2 * entity_capacity + index] = byte(flags);
        view.tags[3 * entity_capacity + index] = byte(auxiliary);
        view.ids[index] = word(token.ids[1]);
        view.ids[entity_capacity + index] = word(token.ids[3]);
        view.ids[2 * entity_capacity + index] = word(token.ids[4]);
        view.ids[3 * entity_capacity + index] = word(token.ids[5]);
        for (int row = 0; row < value_rows; ++row) view.values[row * entity_capacity + index] = word(values[row]);
        return index + 1;
    }
};
SPIRE_FN inline Token token(Kind kind, int id = 0, Zone zone = Zone::None, int position = 0, int owner = 0) {
    Token t{};
    t.ids[0] = static_cast<int>(kind); t.ids[1] = id; t.ids[2] = static_cast<int>(zone);
    t.ids[3] = owner; t.ids[4] = position;
    return t;
}
SPIRE_FN inline bool less(const Token& a, const Token& b) {
    for (int i = 0; i < 6; ++i) if (a.ids[i] != b.ids[i]) return a.ids[i] < b.ids[i];
    for (int i = 0; i < 14; ++i) if (a.values[i] != b.values[i]) return a.values[i] < b.values[i];
    return false;
}
}
