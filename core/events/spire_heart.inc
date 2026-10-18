#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct SpireHeart : EventHooks {
    SPIRE_STATE(SpireHeart, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.beyond.SpireHeart", false}; }
    SPIRE_SNAPSHOT("SpireHeart.screen", scalar, event.state.SpireHeart.screen == 0 ? "INTRO" : event.state.SpireHeart.screen == 1 ? "MIDDLE" : event.state.SpireHeart.screen == 2 ? "MIDDLE_2" : event.state.SpireHeart.screen == 3 ? "DEATH" : "GO_TO_ENDING")
    static SPIRE_FN void enter(Game& g) { event_options(g, 1); }
    static SPIRE_FN void choose(Game& g, int) {
        auto& s = g.event.state.SpireHeart;
        if (s.screen < 2) ++s.screen;
        else if (s.screen == 2) s.screen = g.run.ruby_key && g.run.emerald_key && g.run.sapphire_key ? 4 : 3;
        else if (s.screen == 3) {
            g.event.has_focus = false;
            g.room_dialog.clear();
            g.player.creature.dying = g.player.creature.dead = true;
            g.screen = Screen::Death;
        } else {
            g.room_dialog.clear();
            g.event.has_focus = false;
            next_act(g);
        }
    }
};
}
