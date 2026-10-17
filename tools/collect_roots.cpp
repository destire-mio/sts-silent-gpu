#include "core/engine.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
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
volatile std::sig_atomic_t stop_requested = 0;
constexpr int path_cap = 8192;
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
    double wall_limit;
    Journal* journal;
    const std::clock_t begin = std::clock();
    const std::chrono::steady_clock::time_point wall_begin = std::chrono::steady_clock::now();
    double cpu() const { return double(std::clock() - begin) / CLOCKS_PER_SEC; }
    bool available(uint64_t end) const { return !stop_requested && used < std::min(end, limit) && std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_begin).count() < wall_limit; }
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
bool frame_dependent_preview(const Game& g, const Action& action) {
    return g.screen == Screen::Grid && g.grid_for_transform && !g.grid_confirm_screen &&
        action.kind == ActionKind::Choose && card_rarity(get_card(g,
            g.grid_source == PileId::MasterDeck ? -get_pile(g, g.grid_source)[action.index] - 2 :
            get_pile(g, g.grid_source)[action.index]).id) == CardRarity::Curse;
}
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
        if (g.screen == Screen::CombatReward) return 0.3;
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
        const auto kind = g.reward_objects[g.screen_rewards[a.index]].kind;
        if (kind == RewardKind::Potion) {
            bool empty = false;
            for (int i = 0; i < g.run.potion_slots; ++i) empty |= g.run.potions[i] == PotionId::Empty;
            if (!empty) return 0.001;
        }
        return kind == RewardKind::SapphireKey ? 100 : 10;
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
        if (index >= 11) {
            bool empty = false;
            for (int i = 0; i < g.run.potion_slots; ++i) empty |= g.run.potions[i] == PotionId::Empty;
            if (!empty) return 0.001;
        }
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
bool emerald_reachable(const Game& g, int index) {
    if (index == 105) return false;
    bool reachable[15][7]{};
    for (int y = 14; y >= index / 7; --y) for (int x = 0; x < 7; ++x) {
        const auto& n = g.run.map[y][x];
        reachable[y][x] = n.emerald;
        if (y < 14) for (int next = 0; next < 7; ++next)
            if (n.edges & (1 << next)) reachable[y][x] |= reachable[y + 1][next];
    }
    return reachable[index / 7][index % 7];
}
int pick(const Game& g, const ActionSet& actions, RandomXS128& rng, bool uniform) {
    SPIRE_ASSERT(!actions.empty());
    if (uniform) return rng.nextInt(actions.size);
    if (g.screen == Screen::CombatReward && !g.run.sapphire_key)
        for (int i = 0; i < actions.size; ++i)
            if (actions[i].kind == ActionKind::Choose && g.reward_objects[g.screen_rewards[actions[i].index]].kind == RewardKind::SapphireKey) return i;
    if (g.screen == Screen::Room && g.room_kind == RoomKind::Rest && g.run.act >= 3 && !g.run.ruby_key) {
        int choice = 0;
        for (int i = 0; i < g.campfire.options.size; ++i) if (g.campfire.options[i].usable) {
            if (g.campfire.options[i].kind == CampfireOption::Recall)
                for (int j = 0; j < actions.size; ++j)
                    if (actions[j].kind == ActionKind::Choose && actions[j].index == choice) return j;
            ++choice;
        }
    }
    List<uint8_t, 8> map_options{};
    bool pursue_emerald = false;
    if (g.screen == Screen::Map && !g.run.emerald_key) {
        map_choices(g, map_options);
        for (int i = 0; i < map_options.size; ++i) pursue_emerald |= emerald_reachable(g, map_options[i]);
    }
    std::vector<double> weights(actions.size);
    double total = 0, threat = incoming(g);
    for (int i = 0; i < actions.size; ++i) {
        weights[i] = weight(g, actions[i], threat);
        if (frame_dependent_preview(g, actions[i])) weights[i] = 0;
        if (pursue_emerald && (actions[i].kind != ActionKind::Choose || !emerald_reachable(g, map_options[actions[i].index]))) weights[i] = 0;
        total += weights[i];
    }
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
        bool eligible = false;
        for (int i = 0; i < actions.size; ++i) eligible |= !frame_dependent_preview(node.game, actions[i]);
        if (!eligible) return false;
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
        if (children.empty()) {
            if (!finished.empty()) beam.clear();
            break;
        }
        std::stable_sort(children.begin(), children.end(), [](const Node& a, const Node& b) { return score(a.game) > score(b.game); });
        beam.clear();
        for (auto& child : children) {
            bool duplicate = false;
            for (const auto& kept : beam) duplicate |= score(child.game) == score(kept.game);
            if (!duplicate) beam.push_back(std::move(child));
            if (beam.size() == 6) break;
        }
        int hp = 0;
        for (const auto& n : beam) hp = std::max(hp, n.game.player.creature.hp);
        if (std::any_of(finished.begin(), finished.end(), [hp](const Node& n) {
            return n.game.screen != Screen::Death && n.game.player.creature.hp >= hp;
        })) break;
    }
    finished.insert(finished.end(), beam.begin(), beam.end());
    auto& outcomes = finished;
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
void print_root(const std::string& partition, int stage, const Node& node) {
    const auto& g = node.game;
    std::cout << "{\"kind\":\"root\",\"partition\":\"" << partition << "\",\"stage\":\"" << stages[stage]
              << "\",\"stage_index\":" << stage << ",\"root\":{\"seed\":" << g.run.seed << ",\"candidates\":";
    path_json(node.path);
    std::cout << "},\"hp\":" << g.player.creature.hp << ",\"max_hp\":" << g.player.creature.max_hp
              << ",\"keys\":[" << g.run.ruby_key << ',' << g.run.emerald_key << ',' << g.run.sapphire_key << ']'
              << ",\"floor\":" << g.run.floor << ",\"turn\":" << g.turn << ",\"gold\":" << g.run.gold;
    signature(g);
    std::cout << "}\n";
}
void replay_root(int64_t seed, const std::string& partition, int stage,
                 const std::vector<uint16_t>& prefix, Budget& budget) {
    Node node{}; reset(node.game, seed);
    const char* reason = nullptr;
    for (auto candidate : prefix) {
        SPIRE_ASSERT(budget.available(budget.limit));
        ActionSet actions; legal_actions(node.game, actions);
        if (terminal(node.game)) { reason = "terminal_before_boundary"; break; }
        if (candidate >= actions.size) { reason = "candidate_not_legal_after_rule_fix"; break; }
        if (frame_dependent_preview(node.game, actions[candidate])) { reason = "frame_dependent_curse_preview"; break; }
        budget.step(node, candidate, actions);
    }
    if (!reason && !at_stage(node.game, stage)) reason = "boundary_changed_after_rule_fix";
    if (reason) std::cout << "{\"kind\":\"quarantine\",\"seed\":" << seed << ",\"stage\":\"" << stages[stage]
                         << "\",\"reason\":\"" << reason << "\",\"decision\":" << node.path.size() << "}\n";
    else print_root(partition, stage, node);
    std::cout << std::flush;
}
double floor_score(const Game& g) {
    double quality = 0;
    for (int i = 0; i < g.run.master_deck.size; ++i) {
        const auto& c = g.run.master_deck[i];
        quality += card_value(c.id) + 2 * c.upgrades - 4;
    }
    quality -= 4 * std::max(0, int(g.run.master_deck.size) - 25);
    return g.player.creature.hp + 2 * quality + 8 * g.run.relics.size
        + 32 * (g.run.ruby_key + g.run.emerald_key + g.run.sapphire_key);
}
uint64_t build_signature(const Game& g) {
    uint64_t hash = 1469598103934665603ULL;
    for (int i = 0; i < g.run.master_deck.size; ++i) {
        const auto& c = g.run.master_deck[i];
        hash = (hash ^ (int(c.id) + 512 * c.upgrades)) * 1099511628211ULL;
    }
    for (int i = 0; i < g.run.relics.size; ++i) hash = (hash ^ int(g.run.relics[i].id)) * 1099511628211ULL;
    return hash;
}
void collect(int64_t seed, const std::string& partition, Budget& budget, uint64_t family_limit, int trials,
             const std::vector<uint16_t>& prefix, int excluded) {
    uint64_t start = budget.used, end = start + family_limit;
    RandomXS128 rng(uint64_t(seed) ^ 0xe2000001);
    Node node{}; reset(node.game, seed);
    for (auto candidate : prefix) {
        if (!budget.available(end)) return;
        ActionSet actions; legal_actions(node.game, actions);
        SPIRE_ASSERT(candidate < actions.size);
        budget.step(node, candidate, actions);
    }
    Roots roots;
    for (int i = 0; i < stage_count; ++i) roots.found[i] = excluded & (1 << i);
    std::vector<Node> beam{node};
    std::vector<std::vector<Node>> frontier;
    int deaths = 0, wins = 0, max_floor = 0, rollbacks = 0;
    for (int expansion = 0; expansion < 256 && budget.available(end) && !roots.found[7]; ++expansion) {
        std::vector<Node> children;
        for (auto parent : beam) {
            const int floor = parent.game.run.floor;
            if (fighting(parent.game)) advance(parent, rng, budget, end, trials, &roots);
            if (terminal(parent.game)) {
                deaths += parent.game.screen == Screen::Death;
                wins += parent.game.screen == Screen::Victory;
                continue;
            }
            for (int trial = 0; trial < (floor == 0 ? 24 : 4) && budget.available(end); ++trial) {
                Node child = parent;
                for (int decisions = 0; decisions < 512 && budget.available(end); ++decisions) {
                    max_floor = std::max(max_floor, child.game.run.floor);
                    if (terminal(child.game) || child.path.size() == path_cap) {
                        deaths += child.game.screen == Screen::Death;
                        wins += child.game.screen == Screen::Victory;
                        break;
                    }
                    if (child.game.run.floor > floor) { children.push_back(std::move(child)); break; }
                    if (!advance(child, rng, budget, end, trials, &roots)) break;
                }
            }
        }
        if (children.empty()) {
            if (frontier.empty()) break;
            beam = std::move(frontier.back()); frontier.pop_back(); ++rollbacks;
            continue;
        }
        std::stable_sort(children.begin(), children.end(), [](const Node& a, const Node& b) { return floor_score(a.game) > floor_score(b.game); });
        beam.clear();
        std::vector<Node> alternatives;
        std::vector<uint64_t> builds;
        for (auto& child : children) {
            const auto key = build_signature(child.game);
            if (beam.size() < 8 && std::find(builds.begin(), builds.end(), key) == builds.end()) {
                builds.push_back(key); beam.push_back(std::move(child));
            } else alternatives.push_back(std::move(child));
        }
        while (beam.size() < 8 && !alternatives.empty()) {
            beam.push_back(std::move(alternatives.front())); alternatives.erase(alternatives.begin());
        }
        if (alternatives.size() > 8) alternatives.resize(8);
        if (!alternatives.empty()) frontier.push_back(std::move(alternatives));
        if (frontier.size() > 64) frontier.erase(frontier.begin());
    }
    for (const auto& entry : roots.nodes) print_root(partition, entry.first, entry.second);
    std::cout << "{\"kind\":\"family\",\"partition\":\"" << partition << "\",\"seed\":" << seed
              << ",\"decisions\":" << budget.used - start << ",\"roots\":" << roots.nodes.size()
              << ",\"max_floor\":" << max_floor << ",\"deaths\":" << deaths << ",\"wins\":" << wins
              << ",\"rollbacks\":" << rollbacks << ",\"budget_cutoff\":" << (!budget.available(end) ? "true" : "false") << "}\n" << std::flush;
}
void winning(int64_t seed, const std::string& partition, int stage,
             const std::vector<uint16_t>& prefix, Budget& budget, uint64_t run_limit, int runs) {
    SPIRE_ASSERT(stage >= 4 && stage <= 7);
    Node root{}; reset(root.game, seed);
    for (auto candidate : prefix) {
        ActionSet actions; legal_actions(root.game, actions);
        SPIRE_ASSERT(candidate < actions.size);
        budget.step(root, candidate, actions);
    }
    SPIRE_ASSERT(at_stage(root.game, stage));
    for (int run = 0; run < runs && budget.available(budget.limit); ++run) {
        Node node = root;
        RandomXS128 rng(uint64_t(seed) ^ (uint64_t(stage + 1) << 32) ^ (uint64_t(2) << 48) ^ uint64_t(run + 1729));
        const uint64_t begin = budget.used, end = begin + run_limit;
        while (!terminal(node.game) && node.path.size() < path_cap && budget.available(end)) {
            if (!advance(node, rng, budget, end, run < 4 ? 4 : 16, nullptr)) break;
        }
        std::cout << "{\"kind\":\"winning_search\",\"partition\":\"" << partition
                  << "\",\"seed\":" << seed << ",\"stage\":\"" << stages[stage]
                  << "\",\"attempt\":" << run << ",\"decisions\":" << budget.used - begin
                  << ",\"prefix_decisions\":" << prefix.size() << ",\"continuation_decisions\":" << node.path.size() - prefix.size()
                  << ",\"outcome\":\"" << (node.game.screen == Screen::Victory ? "victory" :
                      node.game.screen == Screen::Death ? "death" : "cutoff") << '"';
        if (node.game.screen == Screen::Victory) {
            std::cout << ",\"candidates\":"; path_json(node.path);
        }
        std::cout << "}\n" << std::flush;
    }
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
        int success = 0, death = 0, other_terminal = 0, cutoff = 0, frame_dependent = 0;
        uint64_t start = budget.used;
        const int runs = mode == 0 ? 16 : 4;
        for (int run = 0; run < runs; ++run) {
            Node node = root;
            bool censored = false;
            RandomXS128 rng(uint64_t(seed) ^ (uint64_t(stage + 1) << 32) ^ (uint64_t(mode + 1) << 48) ^ uint64_t(run + 1729));
            const uint64_t end = budget.used + (mode == 0 ? 4096 : 16384);
            while (!terminal(node.game) && !goal(node.game, stage) && node.path.size() < path_cap && budget.available(end)) {
                if (mode == 0) {
                    ActionSet actions; legal_actions(node.game, actions);
                    const int choice = pick(node.game, actions, rng, true);
                    if (frame_dependent_preview(node.game, actions[choice])) { censored = true; break; }
                    budget.step(node, choice, actions);
                } else if (!advance(node, rng, budget, end, 4, nullptr)) { censored = true; break; }
            }
            success += goal(node.game, stage);
            death += node.game.screen == Screen::Death;
            other_terminal += !goal(node.game, stage) && node.game.screen == Screen::Victory;
            cutoff += !censored && !goal(node.game, stage) && !terminal(node.game);
            frame_dependent += censored;
            SPIRE_ASSERT(budget.available(budget.limit));
        }
        std::cout << (mode == 0 ? ",\"uniform\":{" : ",\"search\":{") << "\"runs\":" << runs
                  << ",\"success\":" << success << ",\"death\":" << death << ",\"cutoff\":" << cutoff
                  << ",\"other_terminal\":" << other_terminal << ",\"frame_dependent\":" << frame_dependent
                  << ",\"decisions\":" << budget.used - start << '}';
    }
    std::cout << "}\n" << std::flush;
}
}
int main(int argc, char** argv) {
    SPIRE_ASSERT(argc == 8);
    std::signal(SIGINT, [](int) { stop_requested = 1; });
    const std::string mode = argv[1];
    const uint64_t limit = std::stoull(argv[2]), family_limit = std::stoull(argv[3]);
    const int trials = std::stoi(argv[4]);
    const double wall_limit = std::stod(argv[5]);
    const int fd = open(argv[6], O_RDWR | O_CREAT | O_EXCL, 0600);
    SPIRE_ASSERT(fd >= 0 && ftruncate(fd, sizeof(Journal)) == 0);
    void* mapping = mmap(nullptr, sizeof(Journal), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    SPIRE_ASSERT(mapping != MAP_FAILED);
    Budget budget{0, limit, wall_limit, static_cast<Journal*>(mapping)};
    std::ifstream input(argv[7]); SPIRE_ASSERT(input.is_open());
    const auto wall_start = std::chrono::steady_clock::now();
    int64_t seed;
    std::string partition;
    while (input >> partition >> seed && budget.available(limit)) {
        SPIRE_ASSERT((partition == "train" && seed >= 1000000000 && seed <= 1999999999) ||
                     (partition == "dev" && seed >= 2000000000 && seed <= 2000004095));
        if (mode == "collect") collect(seed, partition, budget, family_limit, trials, {}, 0);
        else {
            SPIRE_ASSERT(mode == "evaluate" || mode == "extend" || mode == "replay" || mode == "winning");
            int stage, count; input >> stage >> count;
            std::vector<uint16_t> prefix(count);
            for (auto& c : prefix) input >> c;
            SPIRE_ASSERT(input.good());
            if (mode == "evaluate") evaluate(seed, stage, prefix, budget);
            else if (mode == "winning") winning(seed, partition, stage, prefix, budget, family_limit, trials);
            else if (mode == "replay") replay_root(seed, partition, stage, prefix, budget);
            else collect(seed, partition, budget, family_limit, trials, prefix, stage);
        }
    }
    std::cout << "{\"kind\":\"cost\",\"decisions\":" << budget.used << ",\"cpu_seconds\":" << budget.cpu()
              << ",\"wall_seconds\":" << std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_start).count()
              << ",\"stop_requested\":" << (stop_requested ? "true" : "false") << "}\n";
    munmap(mapping, sizeof(Journal)); close(fd);
}
