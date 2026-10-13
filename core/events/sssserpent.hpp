#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Sssserpent : EventHooks {
    SPIRE_STATE(Sssserpent, uint8_t screen; uint8_t curse;)
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.Sssserpent", true}; }
    SPIRE_SNAPSHOT("Sssserpent.goldReward", scalar, 150)
    SPIRE_SNAPSHOT("Sssserpent.screen", scalar, event.state.Sssserpent.screen == 0 ? "INTRO" : event.state.Sssserpent.screen == 1 ? "AGREE" : event.state.Sssserpent.screen == 2 ? "DISAGREE" : "COMPLETE")
    SPIRE_SNAPSHOT("Sssserpent.curse", scalar, [&]() -> pybind11::object { const int index = event.state.Sssserpent.curse; for (int i = 0; i < graph.game.run.master_deck.size; ++i) if (graph.game.run.master_deck[i].uuid == graph.game.cards[index].uuid) return graph.master_card_uuid(graph.game.cards[index].uuid); return graph.card(index); }())
    static SPIRE_FN void enter(Game& g) { g.event.state.Sssserpent.curse = create_card(g, make_card(CardId::Doubt)); event_options(g, 2); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Sssserpent;
        if (s.screen == 0) { s.screen = option == 0 ? 1 : 2; event_options(g, 1); }
        else if (s.screen == 1) {
            g.pending_obtains.push_back(s.curse); gain_gold(g, 150); s.screen = 3;
        } else open_map(g);
    }
};
}
