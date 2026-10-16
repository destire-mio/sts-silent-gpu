#include "core/engine.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

using namespace spire;
namespace {
constexpr int path_cap = 4096;
constexpr int stage_count = 9;
constexpr const char* stages[] = {"act2_entry", "act3_entry", "act3_boss", "act4_entry",
                                 "heart_start", "heart_turn2", "heart_turn3", "heart_turn5", "act1_boss"};
struct Node { Game game; std::vector<uint16_t> path; };
struct Journal {
    uint64_t decisions, seed, length;
    uint16_t candidates[path_cap];
};
struct Budget {
    uint64_t used = 0, limit;
    double cpu_limit;
    Journal* journal;
    const std::clock_t begin = std::clock();
    double cpu() const { return double(std::clock() - begin) / CLOCKS_PER_SEC; }
    bool available(uint64_t end) const { return used < std::min(end, limit) && cpu() < cpu_limit; }
    void step(Node& node, int candidate, const ActionSet& actions) {
        SPIRE_ASSERT(node.path.size() < path_cap);
        node.path.push_back(candidate);
        journal->decisions = ++used;
        journal->seed = node.game.run.seed;
        journal->length = node.path.size();
        std::copy(node.path.begin(), node.path.end(), journal->candidates);
        spire::step(node.game, actions[candidate]);
    }
};
bool terminal(const Game& g) { return g.screen == Screen::Death || g.screen == Screen::Victory; }
bool fighting(const Game& g) { return g.room_in_combat && !g.battle_over && !terminal(g); }
bool heart(const Game& g) { return fighting(g) && g.encounter == EncounterId::TheHeart; }
bool at_stage(const Game& g, int stage) {
    if (terminal(g)) return false;
    switch (stage) {
    case 0: return g.run.act == 2 && !g.run.first_room_chosen && g.screen == Screen::Map;
    case 1: return g.run.act == 3 && !g.run.first_room_chosen && g.screen == Screen::Map;
    case 2: return g.run.act == 3 && g.room_kind == RoomKind::Boss && fighting(g);
    case 3: return g.run.act == 4 && !g.run.first_room_chosen && g.screen == Screen::Map;
    case 4: return heart(g);
    case 5: return heart(g) && g.turn == 2;
    case 6: return heart(g) && g.turn == 3;
    case 7: return heart(g) && g.turn == 5;
    case 8: return g.run.act == 1 && g.room_kind == RoomKind::Boss && fighting(g);
    }
    SPIRE_ASSERT(false);
}
bool goal(const Game& g, int stage) {
    switch (stage) {
    case 0: return g.run.act >= 3;
    case 1: return g.run.act > 3 || (g.run.act == 3 && g.room_kind == RoomKind::Boss && fighting(g));
    case 2: return g.run.act >= 4;
    case 3: return heart(g) || g.screen == Screen::Victory;
    case 8: return g.run.act >= 2;
    default: return g.screen == Screen::Victory;
    }
}
double card_value(CardId id) {
    if (card_data(id).type == CardType::Curse || card_data(id).type == CardType::Status) return -12;
    switch (id) {
    case CardId::Adrenaline: case CardId::WraithForm: case CardId::CorpseExplosion: return 15;
    case CardId::AfterImage: case CardId::Footwork: case CardId::Alchemize: return 14;
    case CardId::NoxiousFumes: case CardId::GlassKnife: return 13;
    case CardId::LegSweep: case CardId::DieDieDie: case CardId::BladeDance: case CardId::CripplingPoison:
    case CardId::BouncingFlask: case CardId::PiercingWail: case CardId::WellLaidPlans: case CardId::Malaise:
    case CardId::Terror: return 12;
    case CardId::Catalyst: case CardId::Burst: case CardId::Dash: case CardId::Predator: return 11;
    case CardId::Backflip: case CardId::CloakAndDagger: case CardId::DeadlyPoison: case CardId::CalculatedGamble:
    case CardId::EscapePlan: case CardId::DaggerSpray: case CardId::Backstab: case CardId::Skewer: return 10;
    case CardId::Acrobatics: case CardId::DaggerThrow: case CardId::FlyingKnee: case CardId::Blur:
    case CardId::DodgeAndRoll: case CardId::PoisonedStab: case CardId::Accuracy: return 9;
    case CardId::Neutralize: case CardId::Deflect: case CardId::QuickSlash: return 8;
    case CardId::Survivor: return 7;
    case CardId::Defend_G: case CardId::Prepared: case CardId::Distraction: return 3;
    case CardId::Strike_G: case CardId::Concentrate: case CardId::Reflex: case CardId::Tactician: return 2;
    case CardId::Setup: return 1;
    case CardId::GrandFinale: return 0;
    default: return 5;
    }
}
double incoming(const Game& g) {
    double value = 0;
    for (int i = 0; i < g.monster_order.size; ++i) {
        const auto& m = g.monsters[g.monster_order[i]];
        if (alive(m.creature)) value += std::max(0, int(m.intent_damage)) * std::max(1, int(m.intent_hits));
    }
    return value;
}
double weight(const Game& g, const Action& a, double threat) {
    if (a.kind == ActionKind::Play) {
        const auto& c = g.cards[g.hand[a.index]];
        const double damage = std::max(0, c.damage);
        double value = 2 + damage + 1.3 * std::min(double(std::max(0, c.block)), std::max(0.0, threat - g.player.creature.block));
        if (card_data(c.id).type == CardType::Power) value += g.turn < 3 ? 16 : 5;
        switch (c.id) {
        case CardId::DeadlyPoison: case CardId::BouncingFlask: case CardId::CripplingPoison:
        case CardId::CorpseExplosion: case CardId::Adrenaline: case CardId::Backflip:
        case CardId::Acrobatics: case CardId::Terror: case CardId::Neutralize: value += 12; break;
        default: break;
        }
        if (a.target >= 0) {
            const auto& m = g.monsters[g.monster_order[a.target]].creature;
            if (damage >= m.hp + m.block) value += 25;
            value += 8.0 / std::max(1, m.hp);
        }
        if (c.id == CardId::WraithForm && g.turn < 4 && threat < g.player.creature.hp) value = 1;
        return std::max(0.1, value / std::max(1, int(c.cost_for_turn)));
    }
    if (a.kind == ActionKind::EndTurn) return 0.05;
    if (a.kind == ActionKind::PotionDiscard) return 0.001;
    if (a.kind == ActionKind::PotionUse) return g.player.creature.hp < threat + 20 || g.room_kind == RoomKind::Boss ? 8 : 0.2;
    if (a.kind != ActionKind::Choose) {
        if (g.screen == Screen::CombatReward) return 0.01;
        return a.kind == ActionKind::Skip ? 2 : 1;
    }
    if (g.screen == Screen::CardReward && a.index < g.reward_cards.size) {
        double value = card_value(g.cards[g.reward_cards[a.index]].id) - (g.run.master_deck.size > 20 ? 5 : 2);
        return std::pow(std::max(0.1, value), 2);
    }
    if (g.screen == Screen::Discard || g.screen == Screen::Grid) {
        const bool grid = g.screen == Screen::Grid;
        const auto& pile = grid ? get_pile(g, g.grid_source) : g.hand;
        const int slot = pile[a.index];
        const auto& card = get_card(g, grid && g.grid_source == PileId::MasterDeck ? -slot - 2 : slot);
        bool remove = grid ? g.grid_for_purge || g.grid_for_transform : !g.selection_for_upgrade;
        if (!grid && g.manager.has_current_action) {
            const auto kind = g.action_objects[g.manager.current_action].kind;
            if (kind == EffectKind::RetainCards || kind == EffectKind::NightmareAction || kind == EffectKind::Setup) remove = false;
        }
        return std::pow(std::max(1.0, remove ? 16 - card_value(card.id) : card_value(card.id)), 2);
    }
    if (g.screen == Screen::Map) {
        List<uint8_t, 8> choices{}; map_choices(g, choices);
        const int index = choices[a.index];
        if (index == 105) return 10;
        const auto& node = g.run.map[index / 7][index % 7];
        if (node.emerald && !g.run.emerald_key) return 35;
        switch (node.room) {
        case RoomKind::Rest: return 20;
        case RoomKind::Event: return 16;
        case RoomKind::Shop: return g.run.gold > 150 ? 16 : 3;
        case RoomKind::Elite: return 2;
        default: return 10;
        }
    }
    if (g.screen == Screen::Room && g.room_kind == RoomKind::Rest) {
        int index = 0;
        for (int i = 0; i < g.campfire.options.size; ++i) if (g.campfire.options[i].usable) {
            const auto option = g.campfire.options[i];
            if (index++ != a.index) continue;
            switch (option.kind) {
            case CampfireOption::Rest: return g.player.creature.hp < g.player.creature.max_hp * 0.7 ? 30 : 2;
            case CampfireOption::Smith: return 15;
            case CampfireOption::Recall: return g.run.act == 3 && g.run.node_y == 14 ? 1000000 : 4;
            default: return 5;
            }
        }
    }
    if (g.screen == Screen::CombatReward) {
        return g.reward_objects[g.screen_rewards[a.index]].kind == RewardKind::SapphireKey ? 100 : 10;
    }
    if (g.screen == Screen::Shop) {
        List<uint8_t, 14> choices{}; shop_choices(g, choices);
        int index = choices[a.index];
        if (index == 0) return 30;
        if (index < 8) {
            const auto id = g.cards[index < 6 ? g.shop.colored[index - 1] : g.shop.colorless[index - 6]].id;
            return std::pow(std::max(0.1, card_value(id) - 5), 2);
        }
        if (index < 11 && g.reward_objects[g.shop.relics[index - 8].reward].relic.id == RelicId::PrismaticShard) return 0.01;
        return 8;
    }
    if (g.screen == Screen::BossReward) {
        const auto id = g.reward_objects[g.chest.relics[a.index]].relic.id;
        switch (id) {
        case RelicId::RunicPyramid: return 30;
        case RelicId::PandorasBox: return 25;
        case RelicId::CursedKey: return 24;
        case RelicId::FusionHammer: case RelicId::RunicDome: return 20;
        case RelicId::SneckoEye: return 18;
        case RelicId::Sozu: return 16;
        case RelicId::CoffeeDripper: return 6;
        case RelicId::BustedCrown: return 5;
        default: return 10;
        }
    }
    return 5;
}
int pick(const Game& g, const ActionSet& actions, RandomXS128& rng, bool uniform) {
    SPIRE_ASSERT(!actions.empty());
    if (uniform) return rng.nextInt(actions.size);
    std::vector<double> weights(actions.size);
    double total = 0, threat = incoming(g);
    for (int i = 0; i < actions.size; ++i) total += weights[i] = weight(g, actions[i], threat);
    double draw = rng.nextDouble() * total;
    int chosen = 0;
    while (chosen + 1 < actions.size && draw >= weights[chosen]) draw -= weights[chosen++];
    return chosen;
}
double score(const Game& g) {
    if (g.screen == Screen::Death) return -1000000;
    if (!fighting(g)) return 1000000 + g.player.creature.hp;
    double result = 2 * g.player.creature.hp;
    for (int i = 0; i < g.monster_order.size; ++i) {
        const auto& m = g.monsters[g.monster_order[i]].creature;
        if (!alive(m)) continue;
        result -= 80 + 1.5 * (m.hp + m.block * 0.2);
        result += 2.5 * power_amount(g, m, PowerId::Poison);
    }
    for (int i = 0; i < g.player.creature.powers.size; ++i) {
        const auto& power = g.player.creature.powers.at(g.power_objects, i);
        double coefficient = 0;
        switch (power.id) {
        case PowerId::Dexterity: case PowerId::NoxiousFumes: coefficient = 20; break;
        case PowerId::Strength: coefficient = 10; break;
        case PowerId::AfterImage: coefficient = 30; break;
        case PowerId::Accuracy: coefficient = 12; break;
        case PowerId::RetainCard: coefficient = 15; break;
        default: break;
        }
        result += coefficient * power.amount;
    }
    return result;
}
struct Roots {
    std::array<bool, stage_count> found{};
    std::vector<std::pair<int, Node>> nodes;
    void observe(const Node& node) {
        for (int stage = 0; stage < stage_count; ++stage) if (!found[stage] && at_stage(node.game, stage)) {
            found[stage] = true;
            nodes.emplace_back(stage, node);
        }
    }
};
bool advance(Node& node, RandomXS128& rng, Budget& budget, uint64_t end, int trials, Roots* roots) {
    if (!budget.available(end) || node.path.size() == path_cap) return false;
    if (!fighting(node.game)) {
        ActionSet actions; legal_actions(node.game, actions);
        budget.step(node, pick(node.game, actions, rng, false), actions);
        if (roots) roots->observe(node);
        return true;
    }
    std::vector<Node> beam{node}, finished;
    for (int turn_count = 0; turn_count < 60 && budget.available(end); ++turn_count) {
        std::vector<Node> children;
        for (const auto& parent : beam) {
            for (int trial = 0; trial < trials && budget.available(end); ++trial) {
                Node child = parent;
                int turn = child.game.turn;
                for (int decision = 0; decision < 120 && child.path.size() < path_cap && budget.available(end); ++decision) {
                    ActionSet actions; legal_actions(child.game, actions);
                    budget.step(child, pick(child.game, actions, rng, false), actions);
                    if (roots) roots->observe(child);
                    if (!fighting(child.game) || child.game.turn != turn) break;
                }
                if (!fighting(child.game)) finished.push_back(std::move(child));
                else children.push_back(std::move(child));
            }
        }
        if (children.empty()) break;
        std::stable_sort(children.begin(), children.end(), [](const Node& a, const Node& b) { return score(a.game) > score(b.game); });
        beam.clear();
        beam.push_back(std::move(children[0]));
        for (size_t i = 1; i < children.size(); ++i) {
            if (score(children[i].game) != score(beam[0].game)) { beam.push_back(std::move(children[i])); break; }
        }
        const int hp = beam.size() == 1 ? beam[0].game.player.creature.hp :
            std::max(beam[0].game.player.creature.hp, beam[1].game.player.creature.hp);
        if (std::any_of(finished.begin(), finished.end(), [hp](const Node& n) {
            return n.game.screen != Screen::Death && n.game.player.creature.hp >= hp;
        })) break;
    }
    auto& outcomes = finished.empty() ? beam : finished;
    node = *std::max_element(outcomes.begin(), outcomes.end(), [](const Node& a, const Node& b) { return score(a.game) < score(b.game); });
    return true;
}
void path_json(const std::vector<uint16_t>& path) {
    std::cout << '[';
    for (size_t i = 0; i < path.size(); ++i) std::cout << (i ? "," : "") << path[i];
    std::cout << ']';
}
void signature(const Game& g) {
    std::vector<std::array<int, 3>> deck;
    for (int i = 0; i < g.run.master_deck.size; ++i) {
        const auto& c = g.run.master_deck[i];
        deck.push_back({int(c.id), c.upgrades, c.misc});
    }
    std::sort(deck.begin(), deck.end());
    std::cout << ",\"deck\":[";
    for (size_t i = 0; i < deck.size(); ++i)
        std::cout << (i ? ",[" : "[") << deck[i][0] << ',' << deck[i][1] << ',' << deck[i][2] << ']';
    std::cout << "],\"relics\":[";
    std::vector<int> relics;
    for (int i = 0; i < g.run.relics.size; ++i) relics.push_back(int(g.run.relics[i].id));
    std::sort(relics.begin(), relics.end());
    for (size_t i = 0; i < relics.size(); ++i) std::cout << (i ? "," : "") << relics[i];
    std::cout << "],\"potions\":[";
    for (int i = 0; i < g.run.potion_slots; ++i) std::cout << (i ? "," : "") << int(g.run.potions[i]);
    std::cout << ']';
}
void collect(int64_t seed, const std::string& partition, Budget& budget, uint64_t family_limit, int trials,
             const std::vector<uint16_t>& prefix, int excluded) {
    uint64_t start = budget.used, end = start + family_limit;
    RandomXS128 rng(uint64_t(seed) ^ 0xe2000001);
    Node node{}; reset(node.game, seed);
    for (auto candidate : prefix) {
        SPIRE_ASSERT(budget.available(end));
        ActionSet actions; legal_actions(node.game, actions);
        SPIRE_ASSERT(candidate < actions.size);
        budget.step(node, candidate, actions);
    }
    Roots roots;
    for (int i = 0; i < stage_count; ++i) roots.found[i] = excluded & (1 << i);
    std::vector<Node> checkpoints;
    int deaths = 0, wins = 0, max_floor = 0, rollbacks = 0;
    while (budget.available(end)) {
        if (terminal(node.game) || node.path.size() == path_cap) {
            deaths += node.game.screen == Screen::Death;
            wins += node.game.screen == Screen::Victory;
            if (wins || checkpoints.empty()) break;
            node = std::move(checkpoints.back()); checkpoints.pop_back(); ++rollbacks;
        }
        const int floor = node.game.run.floor;
        if (!advance(node, rng, budget, end, trials, &roots)) break;
        max_floor = std::max(max_floor, node.game.run.floor);
        if (node.game.run.floor != floor && !terminal(node.game)) {
            checkpoints.push_back(node);
            if (checkpoints.size() > 64) checkpoints.erase(checkpoints.begin());
        }
    }
    for (const auto& entry : roots.nodes) {
        const auto& n = entry.second; const auto& g = n.game;
        std::cout << "{\"kind\":\"root\",\"partition\":\"" << partition << "\",\"stage\":\"" << stages[entry.first]
                  << "\",\"stage_index\":" << entry.first << ",\"root\":{\"seed\":" << seed << ",\"candidates\":";
        path_json(n.path);
        std::cout << "},\"hp\":" << g.player.creature.hp << ",\"max_hp\":" << g.player.creature.max_hp
                  << ",\"floor\":" << g.run.floor << ",\"turn\":" << g.turn << ",\"gold\":" << g.run.gold;
        signature(g);
        std::cout << "}\n";
    }
    std::cout << "{\"kind\":\"family\",\"partition\":\"" << partition << "\",\"seed\":" << seed
              << ",\"decisions\":" << budget.used - start << ",\"roots\":" << roots.nodes.size()
              << ",\"max_floor\":" << max_floor << ",\"deaths\":" << deaths << ",\"wins\":" << wins
              << ",\"rollbacks\":" << rollbacks << ",\"budget_cutoff\":" << (!budget.available(end) ? "true" : "false") << "}\n" << std::flush;
}
void evaluate(int64_t seed, int stage, const std::vector<uint16_t>& prefix, Budget& budget) {
    Node root{}; reset(root.game, seed);
    uint64_t begin = budget.used;
    for (auto candidate : prefix) {
        SPIRE_ASSERT(budget.available(budget.limit));
        ActionSet actions; legal_actions(root.game, actions);
        SPIRE_ASSERT(candidate < actions.size);
        budget.step(root, candidate, actions);
    }
    SPIRE_ASSERT(at_stage(root.game, stage));
    uint64_t replay_steps = budget.used - begin;
    std::cout << "{\"kind\":\"difficulty\",\"seed\":" << seed << ",\"stage\":\"" << stages[stage]
              << "\",\"prefix_decisions\":" << replay_steps;
    for (int mode = 0; mode < 2; ++mode) {
        int success = 0, death = 0, cutoff = 0;
        uint64_t start = budget.used;
        const int runs = mode == 0 ? 16 : 4;
        for (int run = 0; run < runs; ++run) {
            Node node = root;
            RandomXS128 rng(uint64_t(seed) ^ (uint64_t(stage + 1) << 32) ^ (uint64_t(mode + 1) << 48) ^ uint64_t(run + 1729));
            const uint64_t end = budget.used + (mode == 0 ? 2048 : 512);
            while (!terminal(node.game) && !goal(node.game, stage) && node.path.size() < path_cap && budget.available(end)) {
                if (mode == 0) {
                    ActionSet actions; legal_actions(node.game, actions);
                    budget.step(node, pick(node.game, actions, rng, true), actions);
                } else advance(node, rng, budget, end, 4, nullptr);
            }
            success += goal(node.game, stage);
            death += !goal(node.game, stage) && terminal(node.game);
            cutoff += !goal(node.game, stage) && !terminal(node.game);
            SPIRE_ASSERT(budget.available(budget.limit));
        }
        std::cout << (mode == 0 ? ",\"uniform\":{" : ",\"search\":{") << "\"runs\":" << runs
                  << ",\"success\":" << success << ",\"death\":" << death << ",\"cutoff\":" << cutoff
                  << ",\"decisions\":" << budget.used - start << '}';
    }
    std::cout << "}\n" << std::flush;
}
}
int main(int argc, char** argv) {
    SPIRE_ASSERT(argc == 8);
    const std::string mode = argv[1];
    const uint64_t limit = std::stoull(argv[2]), family_limit = std::stoull(argv[3]);
    const int trials = std::stoi(argv[4]);
    const double cpu_limit = std::stod(argv[5]);
    const int fd = open(argv[6], O_RDWR | O_CREAT | O_EXCL, 0600);
    SPIRE_ASSERT(fd >= 0 && ftruncate(fd, sizeof(Journal)) == 0);
    void* mapping = mmap(nullptr, sizeof(Journal), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    SPIRE_ASSERT(mapping != MAP_FAILED);
    Budget budget{0, limit, cpu_limit, static_cast<Journal*>(mapping)};
    std::ifstream input(argv[7]); SPIRE_ASSERT(input.is_open());
    const auto wall_start = std::chrono::steady_clock::now();
    int64_t seed;
    std::string partition;
    while (input >> partition >> seed && budget.available(limit)) {
        if (mode == "collect") collect(seed, partition, budget, family_limit, trials, {}, 0);
        else {
            SPIRE_ASSERT(mode == "evaluate" || mode == "extend");
            int stage, count; input >> stage >> count;
            std::vector<uint16_t> prefix(count);
            for (auto& c : prefix) input >> c;
            SPIRE_ASSERT(input.good());
            if (mode == "evaluate") evaluate(seed, stage, prefix, budget);
            else collect(seed, partition, budget, family_limit, trials, prefix, stage);
        }
    }
    std::cout << "{\"kind\":\"cost\",\"decisions\":" << budget.used << ",\"cpu_seconds\":" << budget.cpu()
              << ",\"wall_seconds\":" << std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_start).count()
              << "}\n";
    munmap(mapping, sizeof(Journal)); close(fd);
}
