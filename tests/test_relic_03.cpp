#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "Relic mismatch: %s\n", message);
        std::exit(1);
    }
}

int main() {
    Game g;
    reset(g, 101);
    g.run.relics.clear();
    equip_relic(g, RelicId::Mango);
    require(g.player.creature.max_hp == 80 && g.player.creature.hp == 73, "Mango increases and heals max HP");
    reset(g, 101);
    g.run.relics.clear();
    equip_relic(g, RelicId::MarkOfTheBloom);
    equip_relic(g, RelicId::Mango);
    require(g.player.creature.max_hp == 80 && g.player.creature.hp == 59, "Bloom blocks Mango healing, not max HP");
    g.player.creature.hp = 10;
    equip_relic(g, RelicId::MealTicket);
    relic_justEnteredRoom(g, 2, static_cast<int>(RoomKind::Shop));
    require(g.player.creature.hp == 10, "Bloom blocks shop healing");
    g.run.relics.erase(0);
    relic_justEnteredRoom(g, 1, static_cast<int>(RoomKind::Shop));
    require(g.player.creature.hp == 25, "Meal Ticket heals on shop entry");
    relic_justEnteredRoom(g, 1, static_cast<int>(RoomKind::Monster));
    require(g.player.creature.hp == 25, "Meal Ticket does not heal on monster entry");
    g.room_kind = RoomKind::Shop;
    equip_relic(g, RelicId::MawBank);
    require(!relic_canSpawn(g, 2), "Maw Bank cannot spawn in a shop");
    relic_onEnterRoom(g, 2, static_cast<int>(RoomKind::Monster));
    require(g.run.gold == 111, "Maw Bank grants gold before being spent");
    lose_gold(g, 1);
    relic_onEnterRoom(g, 2, static_cast<int>(RoomKind::Monster));
    require(g.run.gold == 110 && g.run.relics[2].counter == -2, "Maw Bank stops after spending");
    reset(g, 101);
    g.run.relics.clear();
    const int attack = create_card(g, make_card(CardId::Strike_G));
    const int skill = create_card(g, make_card(CardId::Defend_G));
    const int selection = create_card(g, make_card(CardId::Strike_G));
    RoomReward reward{};
    reward.cards.push_back(attack);
    reward.cards.push_back(skill);
    g.screen_rewards.push_back(create_reward(g, reward));
    g.reward_cards.push_back(selection);
    equip_relic(g, RelicId::MoltenEgg2);
    require(g.cards[attack].upgrades == 1 && g.cards[attack].base_damage == 9, "Molten Egg upgrades an existing attack reward");
    require(g.cards[skill].upgrades == 0 && g.cards[selection].upgrades == 0, "Molten Egg leaves skills and discovery cards alone");
    obtain_card(g, make_card(CardId::Strike_G));
    require(g.run.master_deck[g.run.master_deck.size - 1].upgrades == 1, "Molten Egg upgrades an acquired attack");
    g.run.floor = 49;
    require(!relic_canSpawn(g, 0), "Molten Egg floor cutoff");
    return 0;
}
