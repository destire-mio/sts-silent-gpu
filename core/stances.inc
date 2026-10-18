#pragma once
#include "core/stances/neutral.hpp"
#include "core/stances/wrath.hpp"
#include "core/stances/calm.hpp"
#include "core/stances/divinity.hpp"
#include "core/stances/switched_stance.hpp"
#include "core/stances/on_stance_change.hpp"

namespace spire {
SPIRE_FN inline float stance_damage_give(const Game& g, float value, DamageType type) {
    if (g.player.stance == Stance::Wrath) return units::stances::Wrath::damageGive(value, type);
    if (g.player.stance == Stance::Divinity) return units::stances::Divinity::damageGive(value, type);
    return value;
}
SPIRE_FN inline float stance_damage_receive(const Game& g, float value, DamageType type) {
    return g.player.stance == Stance::Wrath ? units::stances::Wrath::damageReceive(value, type) : value;
}
SPIRE_FN inline void stance_start_turn(Game& g) {
    if (g.player.stance == Stance::Divinity) units::stances::Divinity::atStartOfTurn(g);
}
SPIRE_FN inline void change_stance(Game& g, Stance stance) {
    const Stance old = g.player.stance;
    if (old != stance) {
        Powers& powers = g.player.creature.powers;
        for (int p = next_power_onChangeStance(g, powers.head); p != 255; p = next_power_onChangeStance(g, g.power_objects[p].next)) power_onChangeStance(g, g.power_objects[p], player_target, static_cast<int>(old), static_cast<int>(stance));
        for (int i = next_relic_onChangeStance(g, 0); i < g.run.relics.size; i = next_relic_onChangeStance(g, i + 1)) relic_onChangeStance(g, i, static_cast<int>(old), static_cast<int>(stance));
        if (old == Stance::Calm) units::stances::Calm::onExit(g);
        g.player.stance = stance;
        if (stance == Stance::Divinity) units::stances::Divinity::onEnter(g);
        ++g.stance_changes[static_cast<int>(stance)];
        switched_stance(g);
        for (int i = next_card_triggerExhaustedCardsOnStanceChange(g, g.discard, 0); i < g.discard.size; i = next_card_triggerExhaustedCardsOnStanceChange(g, g.discard, i + 1)) card_triggerExhaustedCardsOnStanceChange(g, g.discard[i], static_cast<int>(stance));
        on_stance_change(g, stance);
    }
    refresh_powers(g);
}
}
