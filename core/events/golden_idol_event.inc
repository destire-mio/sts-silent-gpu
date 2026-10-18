#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct GoldenIdolEvent : EventHooks {
    SPIRE_STATE(GoldenIdolEvent, int damage; int maxHpLoss; uint8_t screenNum; uint8_t relicMetric;)
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.GoldenIdolEvent", true}; }
    SPIRE_SNAPSHOT("GoldenIdolEvent.relicMetric", scalar, event.state.GoldenIdolEvent.relicMetric ? graph.relic(event.state.GoldenIdolEvent.relicMetric - 1) : pybind11::object(pybind11::none()))
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.GoldenIdolEvent;
        s.damage = static_cast<int>(g.player.creature.max_hp * 0.35f);
        s.maxHpLoss = max(1, static_cast<int>(g.player.creature.max_hp * 0.1f));
        event_options(g, 2);
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.GoldenIdolEvent;
        if (s.screenNum == 0) {
            if (option == 0) {
                equip_relic(g, has_relic(g, RelicId::GoldenIdol) ? RelicId::Circlet : RelicId::GoldenIdol);
                s.relicMetric = g.run.relics.size;
                s.screenNum = 1; event_options(g, 3);
            } else { s.screenNum = 2; event_options(g, 1); }
        } else if (s.screenNum == 1) {
            if (option == 0) queue_card_obtain(g, make_card(CardId::Injury));
            else if (option == 1) event_damage(g, s.damage);
            else decrease_max_hp(g, s.maxHpLoss);
            s.screenNum = 2; event_options(g, 1);
        } else open_map(g);
    }
};
}
