#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Parasite : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Curse, CardTarget::None, -2, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game&, int, int) {}
    static SPIRE_FN void onRemoveFromMasterDeck(Game& g, Card&) {
        Creature& player = g.player.creature;
        player.max_hp = max(1, player.max_hp - 3);
        player.hp = min(player.hp, player.max_hp);
    }
    static SPIRE_FN void upgrade(Card&) {}
};
}
