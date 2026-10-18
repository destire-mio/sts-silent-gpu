#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct WindingHalls : EventHooks {
    SPIRE_STATE(WindingHalls, int hpAmt; int healAmt; int maxHPAmt; uint8_t screenNum;)
    static SPIRE_FN constexpr EventData data() { return {"events.beyond.WindingHalls", true}; }
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.WindingHalls;
        s.hpAmt = floor_int(g.player.creature.max_hp * 0.18f + 0.5f);
        s.healAmt = floor_int(g.player.creature.max_hp * 0.2f + 0.5f);
        s.maxHPAmt = floor_int(g.player.creature.max_hp * 0.05f + 0.5f);
        event_options(g, 1);
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.WindingHalls;
        if (s.screenNum == 0) { s.screenNum = 1; event_options(g, 3); }
        else if (s.screenNum == 1) {
            if (option == 0) {
                event_damage(g, s.hpAmt);
                queue_card_obtain(g, make_card(CardId::Madness)); queue_card_obtain(g, make_card(CardId::Madness));
            } else if (option == 1) { heal_creature(g, player_target, s.healAmt); queue_card_obtain(g, make_card(CardId::Writhe)); }
            else decrease_max_hp(g, s.maxHPAmt);
            s.screenNum = 2; event_options(g, 1);
        } else open_map(g);
    }
};
}
