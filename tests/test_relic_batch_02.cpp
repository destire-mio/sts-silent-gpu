#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Relic mismatch: %s\n", message);
        std::exit(1);
    }
}

int main() {
    Game g{};
    reset(g, 101);
    g.run.relics.clear();
    g.run.master_deck.clear();
    g.player.creature.hp = 40;
    equip_relic(g, RelicId::DarkstonePeriapt);
    equip_relic(g, RelicId::DuVuDoll);
    obtain_card(g, make_card(CardId::Injury));
    require(g.player.creature.max_hp == 72 && g.player.creature.hp == 46, "curse increases maximum and current HP");
    require(g.run.relics[1].counter == 1, "curse acquisition updates Du-Vu Doll");
    obtain_card(g, make_card(CardId::Strike_G));
    require(g.player.creature.max_hp == 72 && g.run.relics[1].counter == 1, "attack acquisition leaves curse effects unchanged");
    remove_master_card(g, 0);
    require(g.run.relics[1].counter == 0, "curse removal updates Du-Vu Doll");
    equip_relic(g, RelicId::FrozenEgg2);
    Card power = make_card(CardId::Footwork);
    preview_card_obtain(g, power);
    obtain_card(g, power);
    require(g.run.master_deck[1].upgrades == 1, "preview plus obtain upgrades a power once");
    require(g.run.master_deck[0].upgrades == 0, "Frozen Egg leaves the attack unchanged");

    equip_relic(g, RelicId::Ectoplasm);
    gain_gold(g, 20);
    require(g.run.gold == 99, "Ectoplasm prevents gold gain");
    unequip_relic(g, RelicId::Ectoplasm);
    gain_gold(g, 20);
    require(g.run.gold == 119 && g.player.energy_master == 3, "unequipping restores gold gains and energy master");

    for (int i = 0; i < 8; ++i) g.run.master_deck.push_back(make_card(CardId::Defend_G));
    equip_relic(g, RelicId::EternalFeather);
    relic_onEnterRoom(g, g.run.relics.size - 1, static_cast<int>(RoomKind::Monster));
    require(g.player.creature.hp == 46, "ordinary room does not heal");
    relic_onEnterRoom(g, g.run.relics.size - 1, static_cast<int>(RoomKind::Rest));
    require(g.player.creature.hp == 52, "ten cards heal six HP on rest room entry");

    equip_relic(g, RelicId::Courier);
    g.run.floor = 48;
    require(relic_canSpawn(g, g.run.relics.size - 1), "Courier can spawn on floor 48");
    g.run.floor = 49;
    require(!relic_canSpawn(g, g.run.relics.size - 1), "Courier cannot spawn on floor 49");
    g.run.floor = 1;
    g.room_kind = RoomKind::Shop;
    require(!relic_canSpawn(g, g.run.relics.size - 1), "Courier cannot spawn in a shop");

    equip_relic(g, RelicId::CoffeeDripper);
    const int coffee = g.run.relics.size - 1;
    equip_relic(g, RelicId::FusionHammer);
    const int hammer = g.run.relics.size - 1;
    require(!relic_canUseCampfireOption(g, coffee, static_cast<int>(CampfireOption::Rest)), "Coffee Dripper prevents rest");
    require(relic_canUseCampfireOption(g, coffee, static_cast<int>(CampfireOption::Smith)), "Coffee Dripper allows smith");
    require(!relic_canUseCampfireOption(g, hammer, static_cast<int>(CampfireOption::Smith)), "Fusion Hammer prevents smith");
    require(relic_canUseCampfireOption(g, hammer, static_cast<int>(CampfireOption::Rest)), "Fusion Hammer allows rest");

    reset(g, 101);
    g.run.relics.clear();
    g.run.master_deck.clear();
    g.run.master_deck.push_back(make_card(CardId::AscendersBane));
    g.run.master_deck.push_back(make_card(CardId::Parasite));
    g.run.master_deck.push_back(make_card(CardId::Strike_G));
    equip_relic(g, RelicId::DuVuDoll);
    equip_relic(g, RelicId::EmptyCage);
    require(g.run.master_deck.size == 1 && g.run.master_deck[0].id == CardId::AscendersBane, "Empty Cage removes two purgeable cards and retains Ascenders Bane");
    require(g.run.relics[0].counter == 1 && g.player.creature.max_hp == 63, "Empty Cage invokes removal and master deck callbacks");
    require(g.run.relics[1].state.EmptyCage.cardsSelected, "automatic cage acquisition completes");
    require(g.room_phase == RoomPhase::Complete, "automatic cage acquisition completes its room");
    relic_onEquip(g, 1);
    require(g.room_phase == RoomPhase::Incomplete && g.run.relics[1].state.EmptyCage.cardsSelected,
        "EmptyCage.onEquip leaves an empty purgeable deck's room incomplete");
    equip_relic(g, RelicId::CursedKey);
    const int curses = g.rng.card.counter;
    relic_onChestOpen(g, 2, true);
    require(g.rng.card.counter == curses && g.pending_obtains.empty(), "boss chest does not produce a curse");
    relic_onChestOpen(g, 2, false);
    require(g.rng.card.counter == curses + 1 && g.run.master_deck.size == 1 && g.pending_obtains.size == 1,
        "ordinary chest draws a curse on cardRng and schedules acquisition");
    settle_card_obtains(g);
    require(g.run.master_deck.size == 2 && g.run.relics[0].counter == 2 && g.pending_obtains.empty(),
        "scheduled curse acquisition invokes deck hooks on settlement");
    std::printf("sizeof(Game)=%zu\n", sizeof(Game));
}
