#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

int main() {
    Game g{};
    reset(g, 101);
    CombatSetup setup{};
    setup.deck.push_back(make_card(CardId::Strike_G));
    setup.potions[0] = PotionId::EntropicBrew;
    setup.potions[1] = PotionId::BlockPotion;
    start_combat(g, setup);
    require(potion_canUse(g, 0) && potion_canUse(g, 1), "combat permits both potions");
    g.event.id = EventId::WeMeetAgain;
    require(!potion_canUse(g, 0) && !potion_canUse(g, 1) && !potion_canDiscard(g, 0) && !potion_canDiscard(g, 1),
            "We Meet Again blocks potion use and discard");
    ActionSet legal{};
    legal_actions(g, legal);
    for (int i = 0; i < legal.size; ++i)
        require(legal[i].kind != ActionKind::PotionUse && legal[i].kind != ActionKind::PotionDiscard,
                "blocked potions produce no input actions");
    g.event.id = EventId::Lab;
    g.room_in_combat = false;
    require(potion_canUse(g, 0) && !potion_canUse(g, 1) && potion_canDiscard(g, 0) && potion_canDiscard(g, 1),
            "Entropic Brew remains usable in other event rooms");
    g.manager.turn_has_ended = true;
    require(potion_canUse(g, 0), "an ended combat turn does not block Entropic Brew outside combat");
    Game expected = g;
    const PotionId first = random_potion(expected, false);
    step(g, {ActionKind::PotionUse, 0, -1});
    require(g.run.potions[0] == first && g.run.potions[1] == PotionId::BlockPotion,
            "outside combat, Brew fills the consumed slot and preserves the occupied slot");
    g.run.potions[0] = PotionId::EntropicBrew;
    g.room_in_combat = true;
    require(!potion_canUse(g, 0), "an ended turn blocks Entropic Brew in combat");
    start_combat(g, setup);
    require(g.room_event_class == 0 && potion_canUse(g, 0) && potion_canUse(g, 1),
            "a new combat clears the previous event restriction");
}
