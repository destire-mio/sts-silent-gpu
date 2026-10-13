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

static void test_campfire_hooks() {
    Game g{};
    reset(g, 101);
    g.run.relics.clear();
    equip_relic(g, RelicId::Girya);
    CampfireOptions options{};
    relic_addCampfireOption(g, 0, options);
    require(options.size == 1 && options[0].kind == CampfireOptionKind::Lift && options[0].usable, "Girya offers lifting");
    g.run.relics[0].counter = 3;
    options.clear();
    relic_addCampfireOption(g, 0, options);
    require(!options[0].usable, "Girya stops at three lifts");
    g.run.master_deck.clear();
    g.run.master_deck.push_back(make_card(CardId::AscendersBane));
    equip_relic(g, RelicId::PeacePipe);
    options.clear();
    relic_addCampfireOption(g, 1, options);
    require(options[0].kind == CampfireOptionKind::Toke && !options[0].usable, "Ascender's Bane cannot be purged");
    Card card = make_card(CardId::Strike_G);
    card.in_bottle_flame = true;
    g.run.master_deck.push_back(card);
    options.clear();
    relic_addCampfireOption(g, 1, options);
    require(!options[0].usable, "Peace Pipe excludes bottled cards");
    g.run.master_deck[1].in_bottle_flame = false;
    options.clear();
    relic_addCampfireOption(g, 1, options);
    require(options[0].usable, "Peace Pipe offers purging an ordinary card");
    equip_relic(g, RelicId::Shovel);
    options.clear();
    relic_addCampfireOption(g, 2, options);
    require(options[0].kind == CampfireOptionKind::Dig && options[0].usable, "Shovel offers digging");
    g.run.floor = 47;
    require(!relic_canSpawn(g, 0) && !relic_canSpawn(g, 1) && !relic_canSpawn(g, 2), "two campfire relics prevent another spawn");
    g.run.relics.erase(2);
    g.run.relics.erase(1);
    require(relic_canSpawn(g, 0), "one campfire relic permits a spawn");
    g.run.floor = 48;
    require(!relic_canSpawn(g, 0), "Girya excludes floor 48");
}

static void test_equipment_and_counters() {
    Game g{};
    reset(g, 101);
    g.run.relics.clear();
    g.player.creature.hp = 1;
    equip_relic(g, RelicId::Waffle);
    require(g.player.creature.max_hp == 73 && g.player.creature.hp == 73, "Waffle raises max HP and heals to full");
    equip_relic(g, RelicId::TinyChest);
    require(g.run.relics[1].counter == 0, "Tiny Chest resets on equip");
    g.run.floor = 35;
    require(relic_canSpawn(g, 1), "Tiny Chest permits floor 35");
    g.run.floor = 36;
    require(!relic_canSpawn(g, 1), "Tiny Chest excludes floor 36");
    equip_relic(g, RelicId::WingBoots);
    require(g.run.relics[2].counter == 3, "Wing Boots starts with three charges");
    relic_setCounter(g, 2, -2);
    require(g.run.relics[2].state.WingBoots.usedUp && g.run.relics[2].state.WingBoots.grayscale, "Wing Boots consumes its final charge");
    equip_relic(g, RelicId::RingOfTheSerpent);
    require(g.player.master_hand_size == 6, "Serpent adds one card per turn");
    unequip_relic(g, RelicId::RingOfTheSerpent);
    require(g.player.master_hand_size == 5, "Serpent unequip restores hand size");
    equip_relic(g, RelicId::VelvetChoker);
    require(g.player.energy_master == 4, "Choker adds one energy");
    unequip_relic(g, RelicId::VelvetChoker);
    require(g.player.energy_master == 3, "Choker unequip restores energy");
    equip_relic(g, RelicId::ToyOrnithopter);
    g.player.creature.hp = 1;
    relic_onUsePotion(g, g.run.relics.size - 1);
    require(g.player.creature.hp == 6 && g.manager.actions.empty(), "Ornithopter heals outside combat without queueing");
}

static void test_egg_and_lifted_strength() {
    Game g{};
    reset(g, 101);
    g.run.relics.clear();
    equip_relic(g, RelicId::ToxicEgg2);
    Card skill = make_card(CardId::Prepared);
    relic_onObtainCard(g, 0, skill);
    require(skill.upgrades == 1 && skill.magic == 2, "Toxic Egg upgrades an obtained skill");
    relic_onPreviewObtainCard(g, 0, skill);
    require(skill.upgrades == 1, "Toxic Egg does not upgrade twice");
    Card attack = make_card(CardId::Strike_G);
    relic_onObtainCard(g, 0, attack);
    require(attack.upgrades == 0, "Toxic Egg leaves attacks unchanged");
    equip_relic(g, RelicId::Girya);
    g.run.relics[1].counter = 3;
    g.monsters.push_back(construct_monster(g, MonsterId::Cultist));
    g.monster_order.push_back(0);
    g.room_in_combat = true;
    g.screen = Screen::Combat;
    relic_atBattleStart(g, 1);
    run_until_decision(g);
    require(power_amount(g, g.player.creature, PowerId::Strength) == 3, "Girya applies its stored lifts as Strength");
}

int main() {
    test_campfire_hooks();
    test_equipment_and_counters();
    test_egg_and_lifted_strength();
}
