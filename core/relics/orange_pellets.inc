#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct OrangePellets : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    SPIRE_STATIC_STATE(OrangePellets, bool SKILL; bool POWER; bool ATTACK;)
    SPIRE_STATIC("SKILL", game.unit_statics.OrangePellets.SKILL)
    SPIRE_STATIC("POWER", game.unit_statics.OrangePellets.POWER)
    SPIRE_STATIC("ATTACK", game.unit_statics.OrangePellets.ATTACK)
    static SPIRE_FN void atTurnStart(Game& g, int) { g.unit_statics.OrangePellets = {}; }
    static SPIRE_FN void onUseCard(Game& g, int relic, int card, Effect&) {
        auto& s = g.unit_statics.OrangePellets;
        const CardType type = card_data(g.cards[card].id).type;
        if (type == CardType::Attack) s.ATTACK = true;
        else if (type == CardType::Skill) s.SKILL = true;
        else if (type == CardType::Power) s.POWER = true;
        if (!s.ATTACK || !s.SKILL || !s.POWER) return;
        queue_relic_above(g, relic, player_target);
        Effect e{};
        e.kind = EffectKind::RemoveDebuffsAction;
        add_bottom(g, e);
        s = {};
    }
};
}
