#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct ForgottenAltar : EventHooks {
    SPIRE_STATE(ForgottenAltar, int hpLoss;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.ForgottenAltar", true}; }
    static SPIRE_FN void enter(Game& g) {
        g.event.state.ForgottenAltar.hpLoss = floor_int(g.player.creature.max_hp * 0.35f + 0.5f);
        event_options(g, 3); g.image_dialog[0].disabled = !has_relic(g, "Golden Idol");
    }
    static SPIRE_FN void choose(Game& g, int option) {
        if (g.event.screen) { open_map(g); return; }
        if (option == 0) {
            if (has_relic(g, "Bloody Idol")) equip_relic(g, RelicId::Circlet);
            else {
                int index = 0;
                for (int i = 0; i < g.run.relics.size; ++i)
                    if (g.run.relics[i].id == RelicId::GoldenIdol) { index = i; break; }
                relic_onUnequip(g, index);
                g.run.relics[index] = {RelicId::BloodyIdol, relic_data(RelicId::BloodyIdol).counter};
                relic_initialize_state(g.run.relics[index]);
            }
        } else if (option == 1) {
            increase_max_hp(g, 5); event_damage(g, g.event.state.ForgottenAltar.hpLoss);
        } else queue_card_obtain(g, make_card(CardId::Decay));
        g.event.screen = 99; event_options(g, 1);
    }
};
}
