#pragma once
#include "core/engine.hpp"

namespace spire::obs {
constexpr int entity_capacity = 384;
constexpr int candidate_capacity = 128;
constexpr int id_rows = 6;
constexpr int value_rows = 16;
constexpr int candidate_rows = 4;
enum class Kind : int { Screen = 1, Player, Card, Monster, Power, Relic, Potion, Orb, Stance, MapNode, Option, Reward };
enum class Zone : int { None, Master, Hand, Draw, Discard, Exhaust, Selected, Grid, Offer, Shop, Event, History };
enum class Fault : uint32_t { None, Entities = 0xe0000001, Candidates, NoDecision, InvalidCandidate, SeedsExhausted };
enum class Control : int { Bowl = 1, Purge, OpenChest, Merchant, Campfire, EventButton, Spin, CardBack, Boss };

struct Token {
    int32_t ids[id_rows]{};
    int32_t values[value_rows]{};
};
struct View {
    int32_t* ids;
    int32_t* values;
    int32_t* candidates;
    uint8_t* entity_mask;
    uint8_t* candidate_mask;
    SPIRE_FN void clear() const {
        for (int i = 0; i < id_rows * entity_capacity; ++i) ids[i] = 0;
        for (int i = 0; i < value_rows * entity_capacity; ++i) values[i] = 0;
        for (int i = 0; i < candidate_rows * candidate_capacity; ++i) candidates[i] = 0;
        for (int i = 0; i < entity_capacity; ++i) entity_mask[i] = 0;
        for (int i = 0; i < candidate_capacity; ++i) candidate_mask[i] = 0;
    }
};
struct Observation {
    int32_t ids[id_rows * entity_capacity]{};
    int32_t values[value_rows * entity_capacity]{};
    int32_t candidates[candidate_rows * candidate_capacity]{};
    uint8_t entity_mask[entity_capacity]{};
    uint8_t candidate_mask[candidate_capacity]{};
    SPIRE_FN View view() { return {ids, values, candidates, entity_mask, candidate_mask}; }
};
struct Result { uint32_t fault = 0; int entities = 0; int candidates = 0; };
struct Writer {
    View view;
    Result result{};
    SPIRE_FN int add(const Token& token) {
        if (result.fault) return 0;
        if (result.entities == entity_capacity) {
            result.fault = static_cast<uint32_t>(Fault::Entities);
            return 0;
        }
        const int index = result.entities++;
        for (int i = 0; i < id_rows; ++i) view.ids[i * entity_capacity + index] = token.ids[i];
        for (int i = 0; i < value_rows; ++i) view.values[i * entity_capacity + index] = token.values[i];
        view.entity_mask[index] = 1;
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
    for (int i = 0; i < id_rows; ++i) if (a.ids[i] != b.ids[i]) return a.ids[i] < b.ids[i];
    for (int i = 0; i < value_rows; ++i) if (a.values[i] != b.values[i]) return a.values[i] < b.values[i];
    return false;
}
}
