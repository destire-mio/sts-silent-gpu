#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct ScrapOoze : EventHooks {
    SPIRE_STATE(ScrapOoze, int relicObtainChance; int dmg; int totalDamageDealt; int screenNum;)
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.ScrapOoze", true}; }
    static SPIRE_FN void enter(Game& g) { auto& s = g.event.state.ScrapOoze; s.relicObtainChance = 25; s.dmg = 5; event_options(g, 2); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.ScrapOoze;
        if (s.screenNum == 1) { open_map(g); return; }
        if (option == 0) {
            event_damage(g, s.dmg); s.totalDamageDealt += s.dmg;
            if (g.rng.misc.random(0, 99) >= 99 - s.relicObtainChance) {
                const RelicId relic = random_screenless_relic(g, random_relic_tier(g));
                event_options(g, 1); s.screenNum = 1;
                equip_relic(g, relic);
            } else {
                s.relicObtainChance += 10; ++s.dmg;
                // GenericEventDialog.updateDialogOption appends after DeathScreen cleared the dialogs.
                event_options(g, 2);
            }
        } else { event_options(g, 1); s.screenNum = 1; }
    }
};
}
