#pragma once
#include "../hooks.hpp"
#include "../card_pool.hpp"

namespace spire::units::events {
struct Neow : EventHooks {
    enum Reward : uint8_t { RandomColorlessRare, ThreeCards, RandomRareCard, Remove, Upgrade, RandomColorless,
        Transform, ThreePotions, CommonRelic, HpBonus, Gold, Lament, RemoveTwo, TransformTwo,
        RareRelic, ThreeRareCards, LargeGold, LargeHpBonus, BossRelic };
    enum Drawback : uint8_t { None, HpLoss, NoGold, Curse, Damage };
    SPIRE_STATE(Neow, int hp_bonus; int boss_count; uint8_t screen; uint8_t count; uint8_t choices[4]; uint8_t drawbacks[4]; uint8_t activated;)
    static SPIRE_FN constexpr EventData data() { return {"neow.NeowEvent", false}; }
    static SPIRE_FN const char* reward_name(int value) {
        const char* names[] = {"RANDOM_COLORLESS_2", "THREE_CARDS", "ONE_RANDOM_RARE_CARD", "REMOVE_CARD", "UPGRADE_CARD", "RANDOM_COLORLESS", "TRANSFORM_CARD", "THREE_SMALL_POTIONS", "RANDOM_COMMON_RELIC", "TEN_PERCENT_HP_BONUS", "HUNDRED_GOLD", "THREE_ENEMY_KILL", "REMOVE_TWO", "TRANSFORM_TWO_CARDS", "ONE_RARE_RELIC", "THREE_RARE_CARDS", "TWO_FIFTY_GOLD", "TWENTY_PERCENT_HP_BONUS", "BOSS_RELIC"};
        return names[value];
    }
    static SPIRE_FN const char* drawback_name(int value) {
        const char* names[] = {"NONE", "TEN_PERCENT_HP_LOSS", "NO_GOLD", "CURSE", "PERCENT_DAMAGE"};
        return names[value];
    }
    SPIRE_SNAPSHOT("NeowEvent.screenNum", scalar, event.state.Neow.screen)
    SPIRE_SNAPSHOT("NeowEvent.bossCount", scalar, event.state.Neow.boss_count)
    SPIRE_SNAPSHOT("NeowEvent.pickCard", scalar, false)
    SPIRE_SNAPSHOT("NeowEvent.setPhaseToEvent", scalar, true)
    SPIRE_SNAPSHOT("NeowEvent.rewards", event_children, 0, event.state.Neow.count, "neow.NeowReward", [&](int i, const std::string& field) -> pybind11::object { const auto& s = event.state.Neow; if (field == "NeowReward.activated") return graph.scalar((s.activated & (1 << i)) != 0); if (field == "NeowReward.cursed") return graph.scalar(false); if (field == "NeowReward.hp_bonus") return graph.scalar(s.hp_bonus); if (field == "NeowReward.type") return graph.scalar(units::events::Neow::reward_name(s.choices[i])); if (field == "NeowReward.drawback") return graph.scalar(units::events::Neow::drawback_name(s.drawbacks[i])); if (field == "NeowReward.drawbackDef") { if (s.drawbacks[i] == 0) return pybind11::none(); return graph.object({32, 1, i}, "neow.NeowReward$NeowRewardDrawbackDef", [&](const std::string&) { return graph.scalar(units::events::Neow::drawback_name(s.drawbacks[i])); }); } return {}; })
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.Neow;
        s.screen = 1;
        s.boss_count = 1;
        s.hp_bonus = static_cast<int>(g.player.creature.max_hp * 0.1f);
        g.room_phase = RoomPhase::Event;
        g.room_dialog.clear();
        g.room_dialog.push_back(false);
        g.screen = Screen::Event;
    }
    static SPIRE_FN void blessing(Game& g) {
        auto& s = g.event.state.Neow;
        g.run.boss_count = 0;
        if (s.boss_count == 0) {
            s.choices[0] = Lament; s.choices[1] = HpBonus; s.count = 2;
        } else {
            g.rng.neow.reset(g.run.seed);
            g.rng.neow_initialized = true;
            const Reward first[] = {ThreeCards, RandomRareCard, Remove, Upgrade, Transform, RandomColorless};
            const Reward second[] = {ThreePotions, CommonRelic, HpBonus, Lament, Gold};
            s.choices[0] = first[g.rng.neow.random(0, 5)];
            s.choices[1] = second[g.rng.neow.random(0, 4)];
            s.drawbacks[2] = g.rng.neow.random(0, 3) + 1;
            List<Reward, 7> third{};
            third.push_back(RandomColorlessRare);
            if (s.drawbacks[2] != Curse) third.push_back(RemoveTwo);
            third.push_back(RareRelic); third.push_back(ThreeRareCards);
            if (s.drawbacks[2] != NoGold) third.push_back(LargeGold);
            third.push_back(TransformTwo);
            if (s.drawbacks[2] != HpLoss) third.push_back(LargeHpBonus);
            s.choices[2] = third[g.rng.neow.random(0, third.size - 1)];
            g.rng.neow.random(0, 0);
            s.choices[3] = BossRelic; s.count = 4;
        }
        s.screen = 3;
        g.room_dialog.clear();
        for (int i = 0; i < s.count; ++i) g.room_dialog.push_back(false);
    }
    static SPIRE_FN CardId draw_card(Game& g, CardRarity rarity, bool colorless) {
        if (colorless) {
            List<CardId, 64> candidates{};
            for (const auto& entry : colorless_card_pool) {
                const CardId id = card_id(entry.id);
                if (card_rarity(id) != rarity) continue;
                int at = candidates.size;
                while (at > 0 && card_sort_order(candidates[at - 1]) > card_sort_order(id)) --at;
                candidates.insert(at, id);
            }
            return candidates[g.rng.card.random(candidates.size - 1)];
        }
        if (rarity == CardRarity::Rare) return card_id(rare_card_pool[g.rng.neow.random(static_cast<int>(sizeof(rare_card_pool) / sizeof(*rare_card_pool) - 1))].id);
        if (rarity == CardRarity::Uncommon) return card_id(uncommon_card_pool[g.rng.neow.random(static_cast<int>(sizeof(uncommon_card_pool) / sizeof(*uncommon_card_pool) - 1))].id);
        return card_id(common_card_pool[g.rng.neow.random(static_cast<int>(sizeof(common_card_pool) / sizeof(*common_card_pool) - 1))].id);
    }
    static SPIRE_FN void cards(Game& g, bool rare, bool colorless) {
        g.reward_cards.clear();
        for (int i = 0; i < 3; ++i) {
            CardRarity rarity = g.rng.neow.randomBoolean(0.33f) ? CardRarity::Uncommon : CardRarity::Common;
            if (rare) rarity = CardRarity::Rare;
            else if (colorless && rarity == CardRarity::Common) rarity = CardRarity::Uncommon;
            CardId id;
            bool duplicate;
            do {
                id = draw_card(g, rarity, colorless);
                duplicate = false;
                for (int j = 0; j < g.reward_cards.size; ++j) duplicate |= g.cards[g.reward_cards[j]].id == id;
            } while (duplicate);
            g.reward_cards.push_back(create_card(g, make_card(id)));
        }
        g.acquisition_reward = -1;
        g.reward_skippable = true;
        g.screen = Screen::CardReward;
    }
    static SPIRE_FN void activate(Game& g, int choice) {
        auto& s = g.event.state.Neow;
        s.activated |= 1 << choice;
        const int drawback = s.drawbacks[choice];
        if (drawback == HpLoss) {
            g.player.creature.max_hp = max(1, g.player.creature.max_hp - s.hp_bonus);
            g.player.creature.hp = min(g.player.creature.hp, g.player.creature.max_hp);
            g.player.bloodied = g.player.creature.hp <= g.player.creature.max_hp / 2;
        } else if (drawback == NoGold) lose_gold(g, g.run.gold);
        else if (drawback == Damage) {
            Effect info{}; info.source = no_target; info.target = player_target;
            info.damage_type = DamageType::HpLoss; info.amount = g.player.creature.hp / 10 * 3;
            player_damage(g, info);
        }
        switch (s.choices[choice]) {
        case RandomColorlessRare: cards(g, true, true); break;
        case RandomColorless: cards(g, false, true); break;
        case ThreeRareCards: cards(g, true, false); break;
        case ThreeCards: cards(g, false, false); break;
        case RandomRareCard: queue_card_obtain(g, make_card(draw_card(g, CardRarity::Rare, false))); break;
        case CommonRelic: equip_relic(g, relic_id(relic_key_name(random_relic_key(g, 0)))); break;
        case RareRelic: equip_relic(g, relic_id(relic_key_name(random_relic_key(g, 2)))); break;
        case BossRelic: unequip_relic(g, g.run.relics[0].id); equip_relic(g, relic_id(relic_key_name(random_relic_key(g, 4)))); break;
        case Lament: equip_relic(g, RelicId::NeowsLament); break;
        case HpBonus: increase_max_hp(g, s.hp_bonus); break;
        case LargeHpBonus: increase_max_hp(g, s.hp_bonus * 2); break;
        case Gold: gain_gold(g, 100); break;
        case LargeGold: gain_gold(g, 250); break;
        case ThreePotions:
            for (int i = 0; i < 3; ++i) add_potion_reward(g, random_potion_from_pool(g, g.rng.potion));
            open_room_card_rewards(g);
            g.screen_rewards.erase(g.screen_rewards.size - 1);
            g.reward_proceed_visible = true; g.reward_skippable = false;
            g.room_reward_time = true;
            break;
        case Remove: case RemoveTwo: case Transform: case TransformTwo: case Upgrade:
            master_selection_cards(g, -1);
            if (s.choices[choice] == Upgrade) {
                g.grid_cards.clear();
                for (int i = 0; i < g.run.master_deck.size; ++i)
                    if (card_canUpgrade(g, -i - 2)) g.grid_cards.push_back(i);
            }
            open_master_selection(g, s.choices[choice] == RemoveTwo || s.choices[choice] == TransformTwo ? 2 : 1, s.choices[choice] == Remove);
            g.grid_for_upgrade = s.choices[choice] == Upgrade;
            g.grid_for_transform = s.choices[choice] == Transform;
            break;
        }
        if (drawback == Curse) queue_card_obtain(g, make_card(card_id(curse_card_pool[g.rng.card.random(0, 9)].id)));
        settle_card_obtains(g);
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Neow;
        if (s.screen == 1 || s.screen == 0) blessing(g);
        else if (s.screen == 3) {
            g.room_dialog.clear(); g.room_dialog.push_back(false);
            activate(g, option);
            s.screen = 99;
        } else open_map(g);
    }
    static SPIRE_FN void update(Game& g) {
        auto& s = g.event.state.Neow;
        if (g.selected_cards.empty()) return;
        for (int i = 0; i < s.count; ++i) {
            if (!(s.activated & (1 << i))) continue;
            const int type = s.choices[i];
            if (type == Upgrade) card_upgrade(g.run.master_deck[g.selected_cards[0]]);
            else if (type == Remove || type == RemoveTwo || type == Transform || type == TransformTwo) {
                Card originals[2]{};
                const int count = g.selected_cards.size;
                for (int j = 0; j < count; ++j) originals[j] = g.run.master_deck[g.selected_cards[j]];
                if (type == Transform) {
                    Card transformed = transform_card(g, originals[0], false, g.rng.neow);
                    remove_master_card(g, g.selected_cards[0]);
                    queue_card_obtain(g, transformed);
                } else {
                    for (int j = 0; j < count; ++j)
                        for (int k = 0; k < g.run.master_deck.size; ++k)
                            if (g.run.master_deck[k].uuid == originals[j].uuid) { remove_master_card(g, k); break; }
                    if (type == TransformTwo)
                        for (int j = 0; j < count; ++j) queue_card_obtain(g, transform_card(g, originals[j], false, g.rng.neow));
                }
            } else continue;
            g.selected_cards.clear();
            s.activated &= ~(1 << i);
        }
    }
};
}
