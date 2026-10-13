#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Vampires : EventHooks {
    SPIRE_STATE(Vampires, int maxHpLoss; uint8_t screenNum; bool hasVial; bool replaced;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.Vampires", true}; }
    SPIRE_SNAPSHOT("Vampires.bites", scalar, [&]() -> pybind11::object { pybind11::list result; if (event.state.Vampires.replaced) for (int i = 0; i < 5; ++i) result.append("Bite"); return result; }())
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.Vampires;
        s.maxHpLoss = min(g.player.creature.max_hp - 1, -floor_int(-g.player.creature.max_hp * 0.3f));
        s.hasVial = has_relic(g, "Blood Vial");
        event_options(g, s.hasVial ? 3 : 2);
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Vampires;
        if (s.screenNum) { open_map(g); return; }
        if (option == 0 || (option == 1 && s.hasVial)) {
            if (option == 0) decrease_max_hp(g, s.maxHpLoss);
            else unequip_relic(g, RelicId::BloodVial);
            for (int i = g.run.master_deck.size - 1; i >= 0; --i)
                if (g.run.master_deck[i].id == CardId::Strike_G) remove_master_card(g, i);
            for (int i = 0; i < 5; ++i) queue_card_obtain(g, make_card(CardId::Bite));
            s.replaced = true; s.screenNum = 1;
        } else s.screenNum = 2;
        event_options(g, 1);
    }
};
}
