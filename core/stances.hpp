#pragma once
#include "stances/neutral.hpp"
#include "stances/wrath.hpp"
#include "stances/calm.hpp"
#include "stances/divinity.hpp"
#include "stances/switched_stance.hpp"
#include "stances/on_stance_change.hpp"

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
        for (int i = 0; i < powers.size; ++i) power_onChangeStance(g, powers.at(g.power_objects, i), player_target, static_cast<int>(old), static_cast<int>(stance));
        for (int i = 0; i < g.run.relics.size; ++i) relic_onChangeStance(g, i, static_cast<int>(old), static_cast<int>(stance));
        if (old == Stance::Calm) units::stances::Calm::onExit(g);
        g.player.stance = stance;
        if (stance == Stance::Divinity) units::stances::Divinity::onEnter(g);
        ++g.stance_changes[static_cast<int>(stance)];
        switched_stance(g);
        for (int i = 0; i < g.discard.size; ++i) card_triggerExhaustedCardsOnStanceChange(g, g.discard[i], static_cast<int>(stance));
        on_stance_change(g, stance);
    }
    refresh_powers(g);
}
}
