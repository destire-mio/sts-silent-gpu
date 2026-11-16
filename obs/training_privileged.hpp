#pragma once
#include "core/engine.hpp"

namespace spire::obs {
constexpr int privileged_draw_slots = card_capacity;
constexpr int privileged_token_slots = privileged_draw_slots + monster_capacity;
constexpr int privileged_id_bits = 9;

SPIRE_FN SPIRE_INLINE int32_t privileged_token(int id, int detail) {
    return (id + 1) | (detail << privileged_id_bits);
}

SPIRE_FN inline void training_privileged_tokens(const Game& g, int32_t* out) {
    for (int i = 0; i < privileged_token_slots; ++i) out[i] = 0;
    if (g.fault || g.room_phase != RoomPhase::Combat || g.screen == Screen::Death || g.screen == Screen::Victory) return;
    for (int i = 0; i < g.draw.size; ++i) {
        const auto& card = g.cards[g.draw[g.draw.size - 1 - i]];
        out[i] = privileged_token(static_cast<int>(card.id), card.upgrades);
    }

    const bool player_selection = !g.manager.turn_has_ended &&
        (g.screen == Screen::Discard || g.screen == Screen::Grid || g.screen == Screen::CardReward);
    const bool show_intents = (g.screen == Screen::Combat || player_selection) && !has_relic(g, RelicId::RunicDome);
    for (int i = 0; i < g.monster_order.size; ++i) {
        const auto& monster = g.monsters[g.monster_order[i]];
        const auto& creature = monster.creature;
        if (creature.dead || creature.escaped || creature.dying || creature.half_dead) continue;
        const bool pending = monster.pending_move >= 0 &&
            (monster.pending_move != monster.move || monster.pending_intent != monster.intent);
        if (!pending && show_intents && monster.intent != Intent::None) continue;
        const int move = pending ? monster.pending_move : monster.move;
        if (move >= 0) out[privileged_draw_slots + i] = privileged_token(static_cast<int>(monster.id), move + 1);
    }
}
}
