#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Scry : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(Scry, bool selecting;)
    static SPIRE_FN void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.source = e.target = no_target;
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (monsters_basically_dead(g))
            return;
        if (!e.payload.state.Scry.selecting) {
            Powers &powers = g.player.creature.powers;
            for (int p = next_power_onScry(g, powers.head); p != 255; p = next_power_onScry(g, g.power_objects[p].next))
                power_onScry(g, g.power_objects[p], player_target);
            if (g.draw.empty())
                return;
            g.grid_cards.clear();
            const int count = e.amount == -1 ? g.draw.size : min(e.amount, g.draw.size);
            for (int i = 0; i < count; ++i)
                g.grid_cards.push_back(g.draw[g.draw.size - i - 1]);
            open_grid_selection(g, PileId::Grid, e.amount, false);
            g.grid_can_cancel = true;
            g.selection_any_number = true;
            g.selection_can_pick_zero = true;
            e.payload.state.Scry.selecting = true;
            for (int i = next_card_triggerOnScry(g, g.discard, 0); i < g.discard.size; i = next_card_triggerOnScry(g, g.discard, i + 1))
                card_triggerOnScry(g, g.discard[i]);
            return;
        }
        for (int i = 0; i < g.selected_cards.size; ++i)
            draw_pile_to_discard(g, g.selected_cards[i]);
        g.selected_cards.clear();
        // ScryAction.update invokes the discard hooks on all 16 fixed oracle ticks.
        for (int tick = 0; tick < 15; ++tick)
            for (int i = next_card_triggerOnScry(g, g.discard, 0); i < g.discard.size; i = next_card_triggerOnScry(g, g.discard, i + 1))
                card_triggerOnScry(g, g.discard[i]);
    }
};
}
