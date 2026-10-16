#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace spire;

static void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

static Game combat(RelicId first, RelicId second = RelicId::Count) {
    Game g{};
    reset(g, 101);
    CombatSetup setup{};
    for (int i = 0; i < 5; ++i) setup.deck.push_back(make_card(CardId::Defend_G));
    setup.relics.push_back({first});
    if (second != RelicId::Count) setup.relics.push_back({second});
    setup.hp = setup.max_hp = 200;
    start_combat(g, setup);
    return g;
}

static void acquisition_order() {
    Game before = combat(RelicId::Omamori, RelicId::Necronomicon);
    Game after = combat(RelicId::Necronomicon, RelicId::Omamori);
    require(before.run.master_deck.size == 5 && before.run.relics[0].counter == 1,
        "an equipped Omamori prevents the later Necronomicon curse");
    require(after.run.master_deck.size == 6 && after.run.relics[1].counter == 2,
        "a later Omamori leaves an existing curse in the deck");
    obtain_card(before, make_card(CardId::Necronomicurse));
    require(before.run.master_deck.size == 5 && before.run.relics[0].counter == 0 &&
        before.run.relics[0].grayscale && before.run.relics[0].used_up,
        "the second prevented curse exhausts Omamori");
    obtain_card(before, make_card(CardId::Necronomicurse));
    require(before.run.master_deck.size == 6, "an exhausted Omamori permits a curse");
    obtain_card(before, make_card(CardId::Necronomicurse));
    unequip_relic(before, RelicId::Necronomicon);
    require(before.run.master_deck.size == 6 && (before.run.master_deck[5].id == CardId::Necronomicurse),
        "unequipping Necronomicon removes one curse");
    require(before.obtained_cards[static_cast<int>(CardId::Necronomicurse)] == 0,
        "CardHelper does not track curse acquisitions");
}

static void chest_rewards() {
    Game g = combat(RelicId::NlothsMask);
    RoomReward card{};
    card.identity = 10;
    RoomReward first{};
    first.identity = 20;
    first.relic = RelicId::OldCoin;
    first.relic_link = 30;
    RoomReward link{};
    link.identity = 30;
    RoomReward last{};
    last.identity = 40;
    last.relic = RelicId::Pear;
    g.reward_objects.push_back(card);
    g.reward_objects.push_back(first);
    g.reward_objects.push_back(link);
    g.reward_objects.push_back(last);
    for (int i = 0; i < 4; ++i) g.room_rewards.push_back(i);
    relic_onChestOpenAfter(g, 0, true);
    require(g.room_rewards.size == 4 && g.run.relics[0].counter == 1,
        "Nloth's Mask leaves a boss chest untouched");
    relic_onChestOpenAfter(g, 0, false);
    require(g.room_rewards.size == 2 && g.reward_objects[g.room_rewards[0]].identity == 10 &&
        g.reward_objects[g.room_rewards[1]].identity == 40 && g.run.relics[0].counter == -2,
        "Nloth's Mask removes the first relic and its adjacent linked reward");
    relic_onChestOpenAfter(g, 0, false);
    require(g.room_rewards.size == 2 && g.run.relics[0].used_up && g.run.relics[0].grayscale,
        "Nloth's Mask consumes one chest and preserves subsequent rewards");
    g.room_rewards.clear();
    first.relic_link = 40;
    g.reward_objects[1] = first;
    for (int i = 1; i < 4; ++i) g.room_rewards.push_back(i);
    remove_room_relic_reward(g);
    require(g.room_rewards.size == 2 && g.reward_objects[g.room_rewards[0]].identity == 30 && g.reward_objects[g.room_rewards[1]].identity == 40,
        "a nonadjacent linked reward is retained by AbstractRoom.removeOneRelicFromRewards");
}

static void battle_conditions() {
    Game g = combat(RelicId::PreservedInsect);
    Creature& monster = g.monsters[g.monster_order[0]].creature;
    monster.hp = monster.max_hp = 40;
    relic_atBattleStart(g, 0);
    require(monster.hp == 40, "Preserved Insect requires the room's eliteTrigger");
    g.room_elite_trigger = true;
    relic_atBattleStart(g, 0);
    require(monster.hp == 30 && monster.max_hp == 40, "Preserved Insect reduces current health from maximum health");
    monster.hp = 18;
    relic_atBattleStart(g, 0);
    require(monster.hp == 18, "Preserved Insect cannot heal a damaged elite");
    clear_actions(g, g.manager.actions);
    unequip_relic(g, RelicId::PreservedInsect);
    equip_relic(g, RelicId::NeowsLament);
    relic_atBattleStart(g, 0);
    relic_atBattleStart(g, 0);
    relic_atBattleStart(g, 0);
    require(monster.hp == 1 && g.run.relics[0].counter == -2 && g.run.relics[0].used_up,
        "Neow's Lament expires after the third battle");
}

static void energy_and_spawn() {
    Game g = combat(RelicId::PhilosopherStone);
    require(g.player.energy == 4 && g.player.energy_master == 4 && g.player.energy_per_turn == 4,
        "battle preparation copies the equipped energy maximum");
    const int owner = g.monster_order[0];
    require(power_amount(g, g.monsters[owner].creature, PowerId::Strength) == 1,
        "Philosopher's Stone gives existing monsters strength");
    const int spawned = g.monsters.size;
    g.monsters.push_back(construct_monster(g, MonsterId::Cultist));
    relic_onSpawnMonster(g, 0, spawned);
    require(power_amount(g, g.monsters[spawned].creature, PowerId::Strength) == 1,
        "Philosopher's Stone gives a spawned monster strength before group insertion");
    ++g.player.energy_master;
    step(g, {ActionKind::EndTurn});
    require(g.player.energy == 4, "recharge uses the prepared energy value");
    unequip_relic(g, RelicId::PhilosopherStone);
    require(g.player.energy_master == 4, "unequipping removes one energy from the current maximum");
}

static void static_lifetime() {
    Game g = combat(RelicId::OrangePellets);
    Game other = combat(RelicId::OrangePellets);
    Effect use{};
    const int attack = create_card(g, make_card(CardId::Slice));
    relic_onUseCard(g, 0, attack, use);
    unequip_relic(g, RelicId::OrangePellets);
    equip_relic(g, RelicId::OrangePellets);
    apply_power(g, player_target, player_target, PowerId::Weak, 2);
    apply_power(other, player_target, player_target, PowerId::Weak, 2);
    const int skill = create_card(g, make_card(CardId::Deflect));
    const int power = create_card(g, make_card(CardId::Footwork));
    relic_onUseCard(g, 0, skill, use);
    relic_onUseCard(g, 0, power, use);
    const int other_skill = create_card(other, make_card(CardId::Deflect));
    const int other_power = create_card(other, make_card(CardId::Footwork));
    relic_onUseCard(other, 0, other_skill, use);
    relic_onUseCard(other, 0, other_power, use);
    run_until_decision(g);
    run_until_decision(other);
    require(!has_power(g, g.player.creature, PowerId::Weak) && has_power(other, other.player.creature, PowerId::Weak),
        "Orange Pellets static flags survive relic replacement and remain isolated between games");
}

static void reward_upgrades() {
    Game g{};
    reset(g, 101);
    g.run.card_upgraded_chance = 1.0f;
    add_room_card_reward(g);
    const RoomReward& reward = g.reward_objects[g.room_rewards[0]];
    require(reward.cards.size == 3, "the base card reward contains three choices");
    bool upgraded = false;
    for (int i = 0; i < reward.cards.size; ++i) {
        const Card& card = g.cards[reward.cards[i]];
        const bool rare = card_rarity(card.id) == CardRarity::Rare;
        require(card.upgrades == (rare ? 0 : 1), "reward upgrade chance applies to nonrare cards");
        upgraded |= !rare;
    }
    require(upgraded, "the reward covers an upgraded nonrare card");
}

static void acquisition_and_spawn_queries() {
    Game g = combat(RelicId::OldCoin);
    require(g.run.gold == 399, "Old Coin grants 300 gold on acquisition");
    g.player.creature.hp = 33;
    g.player.creature.max_hp = 66;
    g.player.bloodied = true;
    equip_relic(g, RelicId::Pear);
    require(g.player.creature.max_hp == 76 && g.player.creature.hp == 43 && !g.player.bloodied,
        "Pear increases maximum health, heals ten, and updates the bloodied threshold");
    g.run.floor = 48;
    require(relic_canSpawn(g, 0), "Old Coin can spawn on floor 48");
    g.room_kind = RoomKind::Shop;
    require(!relic_canSpawn(g, 0), "Old Coin cannot spawn in a shop");
    g.room_kind = RoomKind::Monster;
    g.run.floor = 49;
    require(!relic_canSpawn(g, 0), "Old Coin cannot spawn after floor 48");
    equip_relic(g, RelicId::PreservedInsect);
    g.run.floor = 52;
    require(relic_canSpawn(g, 2), "Preserved Insect can spawn on floor 52");
    g.run.floor = 53;
    require(!relic_canSpawn(g, 2), "Preserved Insect cannot spawn after floor 52");
}

static void budget_faults() {
    for (int budget = 0; budget < 3; ++budget) {
        const pid_t child = fork();
        require(child >= 0, "fork");
        if (child == 0) {
            rlimit limit{0, 0};
            setrlimit(RLIMIT_CORE, &limit);
            Game g{};
            reset(g, 101);
            if (budget == 0) {
                for (int i = 0; i <= potion_capacity / 2; ++i) equip_relic(g, RelicId::PotionBelt);
            } else if (budget == 1) {
                for (int i = 0; i <= room_reward_capacity; ++i) add_room_card_reward(g);
            } else {
                RoomReward reward{};
                for (int i = 0; i <= reward_card_capacity; ++i) reward.cards.push_back(i);
            }
            std::exit(0);
        }
        int status = 0;
        require(waitpid(child, &status, 0) == child && WIFSIGNALED(status) &&
            (WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP),
            "a relic storage budget overflow aborts in release builds");
    }
}

static void strike_tags() {
    Game g = combat(RelicId::StrikeDummy);
    const int strike = create_card(g, make_card(CardId::SneakyStrike));
    const int slice = create_card(g, make_card(CardId::Slice));
    card_applyPowers(g, strike);
    card_applyPowers(g, slice);
    require(g.cards[strike].damage == 15 && g.cards[slice].damage == 6,
        "Strike Dummy adds damage to Sneaky Strike and leaves an untagged attack untouched");
}

int main() {
    acquisition_order();
    chest_rewards();
    battle_conditions();
    energy_and_spawn();
    static_lifetime();
    reward_upgrades();
    acquisition_and_spawn_queries();
    budget_faults();
    strike_tags();
}
