#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Doppelganger : ActionHooks {
    SPIRE_SNAPSHOT("DoppelgangerAction.p", player)
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_STATE(Doppelganger, bool free_to_play_once; bool upgraded; int energy_on_use;)
    SPIRE_FN static Effect construct(bool upgraded, bool free, int energy_on_use) {
        Effect e{};
        e.kind = EffectKind::Doppelganger;
        action_initialize_state(e);
        e.source = no_target;
        e.target = no_target;
        e.payload.state.Doppelganger.upgraded = upgraded;
        // DoppelgangerAction constructor leaves freeToPlayOnce at false.
        (void)free;
        e.payload.state.Doppelganger.energy_on_use = energy_on_use;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        int amount = g.player.energy;
        if (e.payload.state.Doppelganger.energy_on_use != -1) amount = e.payload.state.Doppelganger.energy_on_use;
        if (has_relic(g, "Chemical X")) amount += 2;
        if (e.payload.state.Doppelganger.upgraded) ++amount;
        if (amount > 0) {
            Effect apply{};
            apply.kind = EffectKind::ApplyPower;
            apply.source = player_target;
            apply.target = player_target;
            apply.amount = amount;
            apply.power = power_id("Energized");
            add_bottom(g, apply);
            apply.power = power_id("Draw Card");
            add_bottom(g, apply);
            if (!e.payload.state.Doppelganger.free_to_play_once) g.player.energy = 0;
        }
    }
};
}
