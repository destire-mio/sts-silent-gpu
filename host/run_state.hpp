#pragma once
#include "core/state.hpp"
#include "core/card_pool.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace spire::host {
struct MapNode {
    std::string room;
    bool emerald = false;
    bool taken = false;
    std::vector<std::array<int, 2>> edges;
    std::vector<std::array<int, 2>> parents;
};

struct FreshRunState {
    std::array<std::array<MapNode, 7>, 15> map;
    std::string bossKey = "The Guardian";
    std::vector<std::string> monsterList, eliteMonsterList, bossList;
    std::vector<std::string> eventList, shrineList, specialOneTimeEventList;
    std::vector<std::string> commonRelicPool, uncommonRelicPool, rareRelicPool;
    std::vector<std::string> shopRelicPool, bossRelicPool, relicsToRemoveOnStart;
    std::vector<std::string> rewardTypes, rewardCards;
    std::string rewardPotion;
    int rewardGold = 0;
    int stolenGold = 0;
    int blizzardPotionMod = 0;
    int cardBlizzRandomizer = 5;
};

inline void initialize_monster_lists(Random& rng, FreshRunState& run) {
    struct Entry { const char* id; float weight; };
    auto normalize = [](std::vector<Entry>& entries) {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.weight < b.weight; });
        float total = 0.0f;
        for (const auto& entry : entries) total += entry.weight;
        for (auto& entry : entries) entry.weight /= total;
    };
    auto roll = [&rng](const std::vector<Entry>& entries) -> std::string {
        const float value = rng.random();
        float cumulative = 0.0f;
        for (const auto& entry : entries) {
            cumulative += entry.weight;
            if (value < cumulative) return entry.id;
        }
        SPIRE_ASSERT(false);
        return {};
    };
    auto fill = [&roll](const std::vector<Entry>& entries, int count, std::vector<std::string>& list, bool elites) {
        const auto size = list.size() + count;
        while (list.size() < size) {
            const auto next = roll(entries);
            if (!list.empty() && list.back() == next) continue;
            if (!elites && list.size() > 1 && list[list.size() - 2] == next) continue;
            list.push_back(next);
        }
    };
    std::vector<Entry> weak = {{"Cultist", 2.0f}, {"Jaw Worm", 2.0f}, {"2 Louse", 2.0f}, {"Small Slimes", 2.0f}};
    normalize(weak);
    fill(weak, 3, run.monsterList, false);
    std::vector<Entry> strong = {{"Blue Slaver", 2.0f}, {"Gremlin Gang", 1.0f}, {"Looter", 2.0f}, {"Large Slime", 2.0f}, {"Lots of Slimes", 1.0f}, {"Exordium Thugs", 1.5f}, {"Exordium Wildlife", 1.5f}, {"Red Slaver", 1.0f}, {"3 Louse", 2.0f}, {"2 Fungi Beasts", 2.0f}};
    normalize(strong);
    for (;;) {
        const auto next = roll(strong);
        const auto& previous = run.monsterList.back();
        if (previous == "2 Louse" && next == "3 Louse") continue;
        if (previous == "Small Slimes" && (next == "Large Slime" || next == "Lots of Slimes")) continue;
        run.monsterList.push_back(next);
        break;
    }
    fill(strong, 12, run.monsterList, false);
    std::vector<Entry> elites = {{"Gremlin Nob", 1.0f}, {"Lagavulin", 1.0f}, {"3 Sentries", 1.0f}};
    normalize(elites);
    fill(elites, 10, run.eliteMonsterList, true);
    run.bossList = {"The Guardian", "The Guardian"};
}

inline void initialize_map(Random& rng, FreshRunState& run) {
    using Position = std::array<int, 2>;
    auto node = [&run](Position p) -> MapNode& { return run.map[p[1]][p[0]]; };
    auto add_edge = [](MapNode& n, Position destination) {
        if (std::find(n.edges.begin(), n.edges.end(), destination) == n.edges.end()) n.edges.push_back(destination);
        std::sort(n.edges.begin(), n.edges.end());
    };
    auto ancestor = [&node](Position first, Position second) -> Position {
        // MapGenerator.getCommonAncestor compares node1.x with node2.y.
        Position left = first[0] < second[1] ? first : second;
        Position right = first[0] < second[1] ? second : first;
        for (int y = first[1]; y >= 0 && y >= first[1] - 5; --y) {
            const auto& lp = node(left).parents;
            const auto& rp = node(right).parents;
            if (lp.empty() || rp.empty()) return {-1, -1};
            left = *std::max_element(lp.begin(), lp.end(), [](Position a, Position b) { return a[0] < b[0]; });
            right = *std::min_element(rp.begin(), rp.end(), [](Position a, Position b) { return a[0] < b[0]; });
            if (left == right) return left;
        }
        return {-1, -1};
    };
    int first = -1;
    for (int path = 0; path < 6; ++path) {
        int x = rng.random(0, 6);
        if (path == 0) first = x;
        while (path == 1 && x == first) x = rng.random(0, 6);
        for (int y = 0; y < 15; ++y) {
            auto& current = run.map[y][x];
            if (y == 14) { add_edge(current, {3, 16}); break; }
            int next = x + rng.random(x == 0 ? 0 : -1, x == 6 ? 0 : 1);
            const auto& parents = run.map[y + 1][next].parents;
            for (const auto& parent : parents) {
                if (parent == Position{x, y}) continue;
                const auto shared = ancestor(parent, {x, y});
                if (shared[1] < 0 || y + 1 - shared[1] >= 3) continue;
                if (next > x) {
                    next = x + rng.random(-1, 0);
                    if (next < 0) next = x;
                } else if (next == x) {
                    next = x + rng.random(-1, 1);
                    if (next > 6) next = x - 1;
                    else if (next < 0) next = x + 1;
                } else {
                    next = x + rng.random(0, 1);
                    if (next > 6) next = x;
                }
            }
            if (x > 0 && !run.map[y][x - 1].edges.empty()) next = std::max(next, run.map[y][x - 1].edges.back()[0]);
            if (x < 6 && !run.map[y][x + 1].edges.empty()) next = std::min(next, run.map[y][x + 1].edges.front()[0]);
            add_edge(current, {next, y + 1});
            run.map[y + 1][next].parents.push_back({x, y});
            x = next;
        }
    }
    std::vector<Position> seen;
    for (auto& n : run.map[0]) {
        std::vector<Position> retained;
        for (const auto& edge : n.edges) {
            if (std::find(seen.begin(), seen.end(), edge) == seen.end()) retained.push_back(edge);
            seen.push_back(edge);
        }
        n.edges = std::move(retained);
    }
    int count = 0;
    for (int y = 0; y < 15; ++y) for (const auto& n : run.map[y]) if (!n.edges.empty() && y != 13) ++count;
    std::vector<std::string> rooms;
    const auto append = [&rooms](float amount, const char* room) { rooms.insert(rooms.end(), static_cast<int>(amount + 0.5f), room); };
    append(static_cast<float>(count) * 0.05f, "ShopRoom");
    append(static_cast<float>(count) * 0.12f, "RestRoom");
    append(static_cast<float>(count) * 0.08f * 1.6f, "MonsterRoomElite");
    append(static_cast<float>(count) * 0.22f, "EventRoom");
    for (auto& n : run.map[14]) n.room = "RestRoom";
    for (auto& n : run.map[0]) n.room = "MonsterRoom";
    for (auto& n : run.map[8]) n.room = "TreasureRoom";
    int unassigned = 0;
    for (auto& row : run.map) for (const auto& n : row) if (!n.edges.empty() && n.room.empty()) ++unassigned;
    while (static_cast<int>(rooms.size()) < unassigned) rooms.push_back("MonsterRoom");
    // RoomTypeAssigner passes Random.random itself: words advance without the wrapper counter.
    for (int i = static_cast<int>(rooms.size()); i > 1; --i) std::swap(rooms[i - 1], rooms[rng.generator.nextInt(i)]);
    for (int y = 0; y < 15; ++y) for (int x = 0; x < 7; ++x) {
        auto& n = run.map[y][x];
        if (n.edges.empty() || !n.room.empty()) continue;
        for (auto it = rooms.begin(); it != rooms.end(); ++it) {
            const auto& room = *it;
            const bool rest = room == "RestRoom", elite = room == "MonsterRoomElite";
            const bool shop = room == "ShopRoom", treasure = room == "TreasureRoom";
            if ((y <= 4 && (rest || elite)) || (y >= 13 && rest)) continue;
            bool matches = false;
            for (const auto& parent : n.parents) {
                if ((rest || elite || shop || treasure) && node(parent).room == room) matches = true;
                for (const auto& edge : node(parent).edges) {
                    if (edge == Position{x, y}) continue;
                    if ((rest || elite || shop || room == "MonsterRoom" || room == "EventRoom") && node(edge).room == room) matches = true;
                }
            }
            if (matches && y != 0) continue;
            n.room = room;
            rooms.erase(it);
            break;
        }
    }
    std::vector<Position> elites;
    for (int y = 0; y < 15; ++y) for (int x = 0; x < 7; ++x) {
        auto& n = run.map[y][x];
        if (!n.edges.empty() && n.room.empty()) n.room = "MonsterRoom";
        if (n.room == "MonsterRoomElite") elites.push_back({x, y});
    }
    node(elites[rng.random(0, static_cast<int>(elites.size()) - 1)]).emerald = true;
}

inline void initialize_relic_pools(Random& rng, FreshRunState& run) {
    struct RelicEntry { const char* id; const char* tier; };
    constexpr RelicEntry shared[] = {
        {"TheAbacus", "SHOP"},
        {"Akabeko", "COMMON"},
        {"Anchor", "COMMON"},
        {"Ancient Tea Set", "COMMON"},
        {"Art of War", "COMMON"},
        {"Astrolabe", "BOSS"},
        {"Bag of Marbles", "COMMON"},
        {"Bag of Preparation", "COMMON"},
        {"Bird Faced Urn", "RARE"},
        {"Black Star", "BOSS"},
        {"Blood Vial", "COMMON"},
        {"Bloody Idol", "SPECIAL"},
        {"Blue Candle", "UNCOMMON"},
        {"Boot", "COMMON"},
        {"Bottled Flame", "UNCOMMON"},
        {"Bottled Lightning", "UNCOMMON"},
        {"Bottled Tornado", "UNCOMMON"},
        {"Bronze Scales", "COMMON"},
        {"Busted Crown", "BOSS"},
        {"Calipers", "RARE"},
        {"Calling Bell", "BOSS"},
        {"CaptainsWheel", "RARE"},
        {"Cauldron", "SHOP"},
        {"Centennial Puzzle", "COMMON"},
        {"CeramicFish", "COMMON"},
        {"Chemical X", "SHOP"},
        {"ClockworkSouvenir", "SHOP"},
        {"Coffee Dripper", "BOSS"},
        {"The Courier", "UNCOMMON"},
        {"CultistMask", "SPECIAL"},
        {"Cursed Key", "BOSS"},
        {"Darkstone Periapt", "UNCOMMON"},
        {"Dead Branch", "RARE"},
        {"DollysMirror", "SHOP"},
        {"Dream Catcher", "COMMON"},
        {"Du-Vu Doll", "RARE"},
        {"Ectoplasm", "BOSS"},
        {"Empty Cage", "BOSS"},
        {"Enchiridion", "SPECIAL"},
        {"Eternal Feather", "UNCOMMON"},
        {"FaceOfCleric", "SPECIAL"},
        {"FossilizedHelix", "RARE"},
        {"Frozen Egg 2", "UNCOMMON"},
        {"Frozen Eye", "SHOP"},
        {"Fusion Hammer", "BOSS"},
        {"Gambling Chip", "RARE"},
        {"Ginger", "RARE"},
        {"Girya", "RARE"},
        {"Golden Idol", "SPECIAL"},
        {"Gremlin Horn", "UNCOMMON"},
        {"GremlinMask", "SPECIAL"},
        {"HandDrill", "SHOP"},
        {"Happy Flower", "COMMON"},
        {"HornCleat", "UNCOMMON"},
        {"Ice Cream", "RARE"},
        {"Incense Burner", "RARE"},
        {"InkBottle", "UNCOMMON"},
        {"Juzu Bracelet", "COMMON"},
        {"Kunai", "UNCOMMON"},
        {"Lantern", "COMMON"},
        {"Letter Opener", "UNCOMMON"},
        {"Lizard Tail", "RARE"},
        {"Mango", "RARE"},
        {"Mark of the Bloom", "SPECIAL"},
        {"Matryoshka", "UNCOMMON"},
        {"MawBank", "COMMON"},
        {"MealTicket", "COMMON"},
        {"Meat on the Bone", "UNCOMMON"},
        {"Medical Kit", "SHOP"},
        {"Membership Card", "SHOP"},
        {"Mercury Hourglass", "UNCOMMON"},
        {"Molten Egg 2", "UNCOMMON"},
        {"Mummified Hand", "UNCOMMON"},
        {"MutagenicStrength", "SPECIAL"},
        {"Necronomicon", "SPECIAL"},
        {"NeowsBlessing", "SPECIAL"},
        {"Nilry's Codex", "SPECIAL"},
        {"Nloth's Gift", "SPECIAL"},
        {"NlothsMask", "SPECIAL"},
        {"Nunchaku", "COMMON"},
        {"Oddly Smooth Stone", "COMMON"},
        {"Odd Mushroom", "SPECIAL"},
        {"Old Coin", "RARE"},
        {"Omamori", "COMMON"},
        {"OrangePellets", "SHOP"},
        {"Orichalcum", "COMMON"},
        {"Ornamental Fan", "UNCOMMON"},
        {"Orrery", "SHOP"},
        {"Pandora's Box", "BOSS"},
        {"Pantograph", "UNCOMMON"},
        {"Peace Pipe", "RARE"},
        {"Pear", "UNCOMMON"},
        {"Pen Nib", "COMMON"},
        {"Philosopher's Stone", "BOSS"},
        {"Pocketwatch", "RARE"},
        {"Potion Belt", "COMMON"},
        {"Prayer Wheel", "RARE"},
        {"PreservedInsect", "COMMON"},
        {"PrismaticShard", "SHOP"},
        {"Question Card", "UNCOMMON"},
        {"Red Mask", "SPECIAL"},
        {"Regal Pillow", "COMMON"},
        {"Runic Dome", "BOSS"},
        {"Runic Pyramid", "BOSS"},
        {"SacredBark", "BOSS"},
        {"Shovel", "RARE"},
        {"Shuriken", "UNCOMMON"},
        {"Singing Bowl", "UNCOMMON"},
        {"SlaversCollar", "BOSS"},
        {"Sling", "SHOP"},
        {"Smiling Mask", "COMMON"},
        {"Snecko Eye", "BOSS"},
        {"Sozu", "BOSS"},
        {"Spirit Poop", "SPECIAL"},
        {"SsserpentHead", "SPECIAL"},
        {"StoneCalendar", "RARE"},
        {"Strange Spoon", "SHOP"},
        {"Strawberry", "COMMON"},
        {"StrikeDummy", "UNCOMMON"},
        {"Sundial", "UNCOMMON"},
        {"Thread and Needle", "RARE"},
        {"Tiny Chest", "COMMON"},
        {"Tiny House", "BOSS"},
        {"Toolbox", "SHOP"},
        {"Torii", "RARE"},
        {"Toxic Egg 2", "UNCOMMON"},
        {"Toy Ornithopter", "COMMON"},
        {"TungstenRod", "RARE"},
        {"Turnip", "RARE"},
        {"Unceasing Top", "RARE"},
        {"Vajra", "COMMON"},
        {"Velvet Choker", "BOSS"},
        {"Lee's Waffle", "SHOP"},
        {"War Paint", "COMMON"},
        {"WarpedTongs", "SPECIAL"},
        {"Whetstone", "COMMON"},
        {"White Beast Statue", "UNCOMMON"},
        {"WingedGreaves", "RARE"},
    };
    constexpr RelicEntry green[] = {
        {"HoveringKite", "BOSS"},
        {"Ninja Scroll", "UNCOMMON"},
        {"Paper Crane", "UNCOMMON"},
        {"Ring of the Serpent", "BOSS"},
        {"Ring of the Snake", "STARTER"},
        {"Snake Skull", "COMMON"},
        {"The Specimen", "RARE"},
        {"Tingsha", "RARE"},
        {"Tough Bandages", "RARE"},
        {"TwistedFunnel", "SHOP"},
        {"WristBlade", "BOSS"},
    };
    const auto bucket = [](const char* id, unsigned capacity) {
        std::uint32_t hash = 0;
        for (; *id; ++id) hash = hash * 31 + static_cast<unsigned char>(*id);
        return (hash ^ (hash >> 16)) & (capacity - 1);
    };
    std::vector<RelicEntry> library;
    const auto add_library = [&library, &bucket](const auto& entries) {
        std::vector<RelicEntry> ordered(std::begin(entries), std::end(entries));
        unsigned capacity = 16;
        while (ordered.size() > capacity * 3 / 4) capacity *= 2;
        std::stable_sort(ordered.begin(), ordered.end(), [&](const auto& a, const auto& b) { return bucket(a.id, capacity) < bucket(b.id, capacity); });
        library.insert(library.end(), ordered.begin(), ordered.end());
    };
    add_library(shared);
    add_library(green);
    const char* tiers[] = {"COMMON", "UNCOMMON", "RARE", "SHOP", "BOSS"};
    std::vector<std::string>* pools[] = {&run.commonRelicPool, &run.uncommonRelicPool, &run.rareRelicPool, &run.shopRelicPool, &run.bossRelicPool};
    for (int i = 0; i < 5; ++i) {
        for (const auto& entry : library) if (std::string(entry.tier) == tiers[i]) pools[i]->push_back(entry.id);
        shuffle(pools[i]->data(), static_cast<int>(pools[i]->size()), rng);
        for (const auto& excluded : run.relicsToRemoveOnStart) {
            const auto found = std::find(pools[i]->begin(), pools[i]->end(), excluded);
            if (found != pools[i]->end()) pools[i]->erase(found);
        }
    }
}

inline void settle_combat_rewards(Game& game, FreshRunState& run) {
    bool escaped = true;
    for (int i = 0; i < game.monster_order.size; ++i) {
        const Monster& monster = game.monsters[game.monster_order[i]];
        escaped &= monster.creature.escaped;
        if (monster.id == MonsterId::Looter && monster.creature.dead)
            run.stolenGold += monster.state.Looter.stolenGold;
    }
    if (run.stolenGold > 0) run.rewardTypes.push_back("STOLEN_GOLD");
    if (!escaped) {
        run.rewardGold = game.rng.treasure.random(10, 20);
        run.rewardTypes.push_back("GOLD");
    }
    struct Potion { const char* id; int rarity; };
    constexpr Potion potions[] = {
        {"Poison Potion", 0},
        {"CunningPotion", 1},
        {"GhostInAJar", 2},
        {"Block Potion", 0},
        {"Dexterity Potion", 0},
        {"Energy Potion", 0},
        {"Explosive Potion", 0},
        {"Fire Potion", 0},
        {"Strength Potion", 0},
        {"Swift Potion", 0},
        {"Weak Potion", 0},
        {"FearPotion", 0},
        {"AttackPotion", 0},
        {"SkillPotion", 0},
        {"PowerPotion", 0},
        {"ColorlessPotion", 0},
        {"SteroidPotion", 0},
        {"SpeedPotion", 0},
        {"BlessingOfTheForge", 0},
        {"Regen Potion", 1},
        {"Ancient Potion", 1},
        {"LiquidBronze", 1},
        {"GamblersBrew", 1},
        {"EssenceOfSteel", 1},
        {"DuplicationPotion", 1},
        {"DistilledChaos", 1},
        {"LiquidMemories", 1},
        {"CultistPotion", 2},
        {"Fruit Juice", 2},
        {"SneckoOil", 2},
        {"FairyPotion", 2},
        {"SmokeBomb", 2},
        {"EntropicBrew", 2},
    };
    const int chance = escaped ? 0 : 40 + run.blizzardPotionMod;
    if (game.rng.potion.random(0, 99) < chance) {
        const int roll = game.rng.potion.random(0, 99);
        const int rarity = roll < 65 ? 0 : roll < 90 ? 1 : 2;
        int index;
        do { index = game.rng.potion.random(32); } while (potions[index].rarity != rarity);
        run.rewardPotion = potions[index].id;
        run.rewardTypes.push_back("POTION");
        run.blizzardPotionMod -= 10;
    } else run.blizzardPotionMod += 10;
    const CardPoolEntry* pools[] = {common_card_pool, uncommon_card_pool, rare_card_pool};
    const int pool_sizes[] = {static_cast<int>(std::size(common_card_pool)), static_cast<int>(std::size(uncommon_card_pool)), static_cast<int>(std::size(rare_card_pool))};
    std::array<int, 3> rarities{};
    for (int i = 0; i < 3; ++i) {
        const int roll = game.rng.card.random(99) + run.cardBlizzRandomizer;
        const int rarity = roll < 3 ? 2 : roll < 40 ? 1 : 0;
        rarities[i] = rarity;
        if (rarity == 2) run.cardBlizzRandomizer = 5;
        if (rarity == 0) run.cardBlizzRandomizer = std::max(-40, run.cardBlizzRandomizer - 1);
        std::string card;
        do { card = pools[rarity][game.rng.card.random(pool_sizes[rarity] - 1)].id; }
        while (std::find(run.rewardCards.begin(), run.rewardCards.end(), card) != run.rewardCards.end());
        run.rewardCards.push_back(card);
    }
    for (const int rarity : rarities) if (rarity != 2) game.rng.card.randomBoolean(0.0f);
    run.rewardTypes.push_back("CARD");
}

inline void initialize_fresh_run(Game& game, FreshRunState& run) {
    run = FreshRunState{};
    initialize_monster_lists(game.rng.monster, run);
    run.eventList = {"Big Fish", "The Cleric", "Dead Adventurer", "Golden Idol", "Golden Wing", "World of Goop", "Liars Game", "Living Wall", "Mushrooms", "Scrap Ooze", "Shining Light"};
    run.shrineList = {"Match and Keep!", "Golden Shrine", "Transmorgrifier", "Purifier", "Upgrade Shrine", "Wheel of Change"};
    run.specialOneTimeEventList = {"Accursed Blacksmith", "Bonfire Elementals", "Designer", "Duplicator", "FaceTrader", "Fountain of Cleansing", "Knowing Skull", "Lab", "N'loth", "SecretPortal", "The Joust", "WeMeetAgain", "The Woman in Blue"};
    // CharacterManager and CharacterSelectScreen each construct the four characters.
    for (int creation = 0; creation < 2; ++creation) {
        for (const char* relic : {"Burning Blood", "Ring of the Snake", "Cracked Core", "PureWater"}) {
            run.relicsToRemoveOnStart.push_back(relic);
        }
    }
    run.relicsToRemoveOnStart.push_back("Ring of the Snake");
    initialize_relic_pools(game.rng.relic, run);
    game.rng.reseed_map(game.run.seed, 1);
    initialize_map(game.rng.map, run);
    // Exordium calls MainMusic.getSong, which consumes miscRng even without audio output.
    game.rng.misc.random(1);
}

}
