#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct PeacePipe : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 300}; d.adds_campfire_option = true; return d; }
    static SPIRE_FN bool canSpawn(const Game& g, int) {
        if (g.run.floor >= 48) return false;
        int count = 0;
        for (int i = 0; i < g.run.relics.size; ++i) {
            const RelicId id = g.run.relics[i].id;
            if (id == RelicId::PeacePipe || id == RelicId::Shovel || id == RelicId::Girya) ++count;
        }
        return count < 2;
    }
    static SPIRE_FN void addCampfireOption(Game& g, int, CampfireOptions& options) {
        bool usable = false;
        for (int i = 0; i < g.run.master_deck.size; ++i) {
            const Card& card = g.run.master_deck[i];
            if ((card.id == CardId::Necronomicurse) || (card.id == CardId::CurseOfTheBell) ||
                (card.id == CardId::AscendersBane) || card.bottle_flame || card.bottle_lightning || card.bottle_tornado) continue;
            usable = true;
        }
        options.push_back({CampfireOption::Toke, usable});
    }
};
}
