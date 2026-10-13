#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Action 03 mismatch: %s\n", message);
        std::exit(1);
    }
}

static Game combat() {
    Game g{};
    reset(g, 17);
    CombatSetup setup{};
    for (int i = 0; i < 5; ++i) setup.deck.push_back(make_card(CardId::Strike_G));
    start_combat(g, setup);
    return g;
}

static void costs_and_energy() {
    Game g = combat();
    g.cards[g.hand[0]].cost = 3;
    g.cards[g.hand[0]].cost_for_turn = 0;
    g.cards[g.hand[1]].cost = 0;
    g.cards[g.hand[1]].cost_for_turn = 4;
    Effect e{};
    e.kind = EffectKind::Enlightenment;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.cards[g.hand[0]].cost == 3 && g.cards[g.hand[0]].cost_for_turn == 0 &&
            g.cards[g.hand[1]].cost == 0 && g.cards[g.hand[1]].cost_for_turn == 1,
            "Enlightenment changes turn cost independently from base cost");
    e.flag = true;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.cards[g.hand[0]].cost == 1 && g.cards[g.hand[0]].cost_for_turn == 0,
            "combat Enlightenment preserves an existing zero turn cost");
    e = Effect{};
    e.kind = EffectKind::GainEnergyIfDiscard;
    e.amount = 5;
    g.player.energy = 998;
    const int energy_total = g.manager.energy_gained_this_combat;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.player.energy == 998 && g.manager.energy_gained_this_combat == energy_total,
            "discard-dependent energy does nothing before a discard");
    g.manager.total_discarded_this_turn = 1;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.player.energy == 999 && g.manager.energy_gained_this_combat == energy_total + 5,
            "energy cap does not cap the combat gain counter");
    e.kind = EffectKind::LoseEnergy;
    e.amount = 1001;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.player.energy == 0 && g.manager.energy_gained_this_combat == energy_total + 5,
            "LoseEnergy clamps at zero without changing the gain counter");
}

static void randomized_costs() {
    Game g = combat();
    for (int i = 0; i < g.hand.size; ++i) {
        g.cards[g.hand[i]].cost = 0;
        g.cards[g.hand[i]].cost_for_turn = 0;
    }
    g.cards[g.hand[4]].cost = 2;
    g.cards[g.hand[4]].cost_for_turn = 0;
    Random expected = g.rng.card_random;
    int chosen;
    do { chosen = expected.random(4); } while (chosen != 4);
    const int64_t before = g.rng.card_random.counter;
    Effect e{};
    e.kind = EffectKind::Madness;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.cards[g.hand[4]].cost == 0 && g.rng.card_random.counter == expected.counter &&
            g.rng.card_random.counter > before + 1,
            "Madness consumes rejected samples from the entire hand");
    const int64_t after = g.rng.card_random.counter;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.rng.card_random.counter == after, "Madness with no eligible card consumes no RNG");
    expected = g.rng.card_random;
    int wanted[5];
    int turns[5];
    for (int i = 0; i < 5; ++i) {
        Card& card = g.cards[g.hand[i]];
        card.cost = i == 0 ? -2 : 1;
        card.cost_for_turn = i == 0 ? -2 : 7;
        wanted[i] = card.cost < 0 ? card.cost : expected.random(3);
        turns[i] = wanted[i] == card.cost ? card.cost_for_turn : wanted[i];
    }
    e.kind = EffectKind::RandomizeHandCost;
    add_bottom(g, e);
    run_until_decision(g);
    for (int i = 0; i < 5; ++i)
        require(g.cards[g.hand[i]].cost == wanted[i] && g.cards[g.hand[i]].cost_for_turn == turns[i],
                "RandomizeHandCost leaves turn cost alone when sampled cost equals base cost");
    require(g.rng.card_random.counter == expected.counter &&
            g.rng.card_random.randomLong() == expected.randomLong(), "cost randomization consumes the exact stream");
}

static void selections() {
    Game g = combat();
    const int first = g.hand[0];
    const int second = g.hand[1];
    g.cards[second].cost = 0;
    Effect e{};
    e.kind = EffectKind::Forethought;
    e.flag = true;
    add_bottom(g, e);
    run_until_decision(g);
    ActionSet legal{};
    legal_actions(g, legal);
    require(g.screen == Screen::HandSelect && !g.manager.current_action.done &&
            legal[legal.size - 1].kind == ActionKind::Proceed, "upgraded Forethought permits zero selections");
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Proceed});
    require(g.draw.size == 2 && g.draw[0] == second && g.draw[1] == first &&
            g.cards[first].free_to_play_once && !g.cards[second].free_to_play_once,
            "Forethought places selections at deck bottom in reverse order and checks base cost");
    e = Effect{};
    e.kind = EffectKind::RetainCards;
    e.amount = 7;
    const int ordinary = g.hand[0];
    const int ethereal = g.hand[1];
    g.cards[ethereal].ethereal = true;
    add_bottom(g, e);
    run_until_decision(g);
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Proceed});
    require(g.cards[ordinary].retain && !g.cards[ethereal].retain && g.hand[g.hand.size - 2] == ordinary &&
            g.hand[g.hand.size - 1] == ethereal, "retain selection restores cards in selection order and excludes ethereal");
    g.retained.push_back(g.hand.erase(g.hand.size - 2));
    const int unretained = create_card(g, make_card(CardId::Defend_G));
    g.retained.push_back(unretained);
    e.kind = EffectKind::RestoreRetained;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.hand[g.hand.size - 1] == ordinary && !g.cards[ordinary].retain && g.retained.size == 1 &&
            g.retained[0] == unretained, "RestoreRetained leaves non-retained entries in the original group");
}

static void damage_and_room_state() {
    Game g = combat();
    Effect e{};
    e.kind = EffectKind::CannotLose;
    add_bottom(g, e);
    run_until_decision(g);
    g.player.creature.dying = true;
    e = Effect{};
    e.kind = EffectKind::Greed;
    e.target = 0;
    e.amount = 1000;
    e.info_base = 1000;
    e.extra = 20;
    const int gold = g.run.gold;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.run.gold == gold + 20 && !g.battle_over && g.cannot_lose && g.screen == Screen::Combat,
            "Greed calls creature damage despite a dying source and cannotLose delays victory");
    e.kind = EffectKind::CanLose;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.screen == Screen::Victory && !g.cannot_lose, "CanLose releases room completion after the last death");
    g = combat();
    g.monsters[0].creature.block = 50;
    const int hp = g.monsters[0].creature.hp;
    e = Effect{};
    e.kind = EffectKind::PoisonLoseHp;
    e.target = 0;
    e.amount = 3;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.monsters[0].creature.hp == hp - 3 && g.monsters[0].creature.block == 50,
            "poison HP loss bypasses block");
    e = units::actions::IncreaseMaxHp::construct(0, 0.1f, false);
    g.monsters[0].creature.hp = 10;
    g.monsters[0].creature.max_hp = 15;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.monsters[0].creature.max_hp == 17 && g.monsters[0].creature.hp == 12,
            "max HP increase rounds half upward and heals by the rounded amount");
    e = Effect{};
    e.kind = EffectKind::Regen;
    e.amount = 4;
    g.player.creature.hp = 50;
    g.battle_over = true;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.player.creature.hp == 54, "Regen still heals in the combat phase after battleOver");
}

static void card_rewards_and_uuid_growth() {
    Game g = combat();
    const int selected = g.hand[0];
    g.discovery_card = selected;
    g.draw.push_back(g.hand.erase(1));
    g.draw.push_back(g.hand.erase(1));
    Effect e{};
    e.kind = EffectKind::Codex;
    action_prepare(g, e);
    e.state.Codex.selecting = true;
    Random expected = g.rng.card_random;
    const int insertion = expected.random(1);
    execute_effect(g, e);
    require(g.draw.size == 3 && g.cards[g.draw[insertion]].id == g.cards[selected].id &&
            g.cards[g.draw[insertion]].uuid != g.cards[selected].uuid && g.discovery_card == -1 &&
            g.rng.card_random.counter == expected.counter, "Codex copies a card and inserts below the draw-pile top");
    e = units::actions::Discovery::construct(false, 2);
    action_prepare(g, e);
    e.state.Discovery.selecting = true;
    g.discovery_card = selected;
    expected = g.rng.card_random;
    for (int update = 0; update < 15; ++update) {
        int samples[3];
        int count = 0;
        while (count < 3) {
            int draw = expected.random(69);
            bool seen = false;
            for (int i = 0; i < count; ++i) if (samples[i] == draw) seen = true;
            if (!seen) samples[count++] = draw;
        }
    }
    const int old_hand = g.hand.size;
    execute_effect(g, e);
    require(g.hand.size == old_hand + 2 && g.cards[g.hand[g.hand.size - 1]].cost_for_turn == 0 &&
            g.rng.card_random.counter == expected.counter && g.rng.card_random.randomLong() == expected.randomLong(),
            "Discovery generates unused choices on each remaining update while adding two free cards");
    g = combat();
    const int in_use = g.hand.erase(0);
    g.player.card_in_use = in_use;
    const uint32_t uuid = g.cards[in_use].uuid;
    const int duplicate = create_card(g, make_card(CardId::Strike_G));
    g.cards[duplicate].uuid = uuid;
    g.hand.push_back(duplicate);
    g.limbo.push_back(duplicate);
    const int orphan = create_card(g, make_card(CardId::Strike_G));
    g.cards[orphan].uuid = uuid;
    g.reward_cards.push_back(orphan);
    DamageInfo info{};
    info.amount = info.info_base = 1000;
    e = units::actions::RitualDagger::construct(0, info, 3, uuid);
    add_bottom(g, e);
    run_until_decision(g);
    require(g.cards[in_use].misc == 3 && g.cards[duplicate].misc == 3 && g.cards[orphan].misc == 0 &&
            g.run.master_deck[uuid - 1].misc == 3, "Ritual Dagger grows each matching battle instance once and the master card");
    require(g.run.master_deck[uuid - 1].damage == 6 && g.run.master_deck[uuid - 1].base_damage == 3,
            "Ritual Dagger applies powers before writing the new base damage");
}

static void source_pools() {
    Game g = combat();
    require(g.run.source_card_pools[0][0] == 18 && g.run.source_card_pools[0][18] == 0 &&
            g.run.source_card_pools[3][0] == 34,
            "source pools preserve the Java addToBottom order");
    for (int i = 0; i < 3; ++i) g.run.source_card_pools[i].clear();
    g.run.source_card_pools[0].push_back(4);
    g.run.source_card_pools[0].push_back(0);
    const int64_t before = g.rng.card_random.counter;
    require(same_id(random_card_in_combat(g, static_cast<int>(CardType::Skill), false), "Cloak And Dagger"),
            "typed combat choices read the source pool and filter attack cards");
    g.run.source_card_pools[3].clear();
    g.run.source_card_pools[3].push_back(8);
    g.run.source_card_pools[3].push_back(34);
    require(same_id(random_card_in_combat(g, -1, true), "Madness") && g.rng.card_random.counter == before + 2,
            "colorless choices exclude healing and consume a draw for a singleton pool");
}

int main() {
    costs_and_energy();
    randomized_costs();
    selections();
    damage_and_room_state();
    card_rewards_and_uuid_growth();
    source_pools();
    std::puts("Action 03 checks passed");
}
