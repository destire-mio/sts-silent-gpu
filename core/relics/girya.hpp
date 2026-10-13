#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Girya : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {0, 300}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        if (g.run.relics[relic].counter == 0) return;
        Effect strength{};
        strength.kind = EffectKind::ApplyPower;
        strength.power = PowerId::Strength;
        strength.amount = g.run.relics[relic].counter;
        add_top(g, strength);
        Effect show{};
        show.kind = EffectKind::RelicAboveCreature;
        show.extra = relic;
        add_top(g, show);
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) {
        if (g.run.floor >= 48) return false;
        int count = 0;
        for (int i = 0; i < g.run.relics.size; ++i) {
            const RelicId id = g.run.relics[i].id;
            if (id == RelicId::PeacePipe || id == RelicId::Shovel || id == RelicId::Girya) ++count;
        }
        return count < 2;
    }
    static SPIRE_FN void addCampfireOption(Game& g, int relic, CampfireOptions& options) {
        options.push_back({CampfireOptionKind::Lift, g.run.relics[relic].counter < 3});
    }
};
}
