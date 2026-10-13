#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct UnceasingTop : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    SPIRE_STATE(UnceasingTop, bool canDraw; bool disabledUntilEndOfTurn;)
    static SPIRE_FN void atPreBattle(Game& g, int relic) { g.run.relics[relic].state.UnceasingTop.canDraw = false; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) {
        auto& state = g.run.relics[relic].state.UnceasingTop;
        state.canDraw = true;
        state.disabledUntilEndOfTurn = false;
    }
    static SPIRE_FN void disableUntilTurnEnds(Game& g, int relic) {
        g.run.relics[relic].state.UnceasingTop.disabledUntilEndOfTurn = true;
    }
    static SPIRE_FN void onRefreshHand(Game& g, int relic) {
        const auto& state = g.run.relics[relic].state.UnceasingTop;
        if (g.manager.actions.empty() && g.hand.empty() && !g.manager.turn_has_ended && state.canDraw &&
            !has_power(g, g.player.creature, "No Draw") && g.screen == Screen::Combat &&
            g.room_in_combat && !state.disabledUntilEndOfTurn && (!g.discard.empty() || !g.draw.empty())) {
            Effect show{};
            show.kind = EffectKind::RelicAboveCreature;
            show.extra = relic;
            add_top(g, show);
            Effect draw{};
            draw.kind = EffectKind::Draw;
            draw.amount = 1;
            add_bottom(g, draw);
        }
    }
};
}
