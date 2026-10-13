#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Power mismatch: %s\n", message);
        std::exit(1);
    }
}

static Game combat() {
    Game g{};
    reset(g, 17);
    CombatSetup setup{};
    for (int i = 0; i < 5; ++i) setup.deck.push_back(make_card(CardId::Defend_G));
    start_combat(g, setup);
    return g;
}

static DamageInfo hit(int source, int target, int amount, DamageType type = DamageType::Normal) {
    DamageInfo info{};
    info.kind = EffectKind::Damage;
    info.source = source;
    info.target = target;
    info.amount = amount;
    info.info_base = amount;
    info.damage_type = type;
    return info;
}

static void test_focus() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::Focus, 1200);
    require(power_amount(g.player.creature, PowerId::Focus) == 1200,
            "Focus constructor leaves the supplied amount unclamped");
    apply_power(g, player_target, player_target, PowerId::Focus, 0);
    require(power_amount(g.player.creature, PowerId::Focus) == 999,
            "Focus stacking clamps at 999");
    apply_power(g, player_target, player_target, PowerId::Focus, -999);
    run_until_decision(g);
    require(!has_power(g.player.creature, PowerId::Focus), "zero Focus stack removes the power");
    apply_power(g, player_target, player_target, PowerId::Focus, -1);
    apply_power(g, player_target, player_target, PowerId::Focus, -1500);
    require(power_amount(g.player.creature, PowerId::Focus) == -999,
            "Focus overrides the inherited nonstacking -1 sentinel");
    const int index = find_power(g.player.creature, PowerId::Focus);
    power_reducePower(g, g.player.creature.powers[index], player_target, -999);
    run_until_decision(g);
    require(!has_power(g.player.creature, PowerId::Focus), "zero Focus reduction removes the power");
    apply_power(g, player_target, player_target, PowerId::Artifact, 1);
    apply_power(g, player_target, player_target, PowerId::Focus, 0);
    run_until_decision(g);
    require(!has_power(g.player.creature, PowerId::Focus) && !has_power(g.player.creature, PowerId::Artifact),
            "zero Focus has DEBUFF type and consumes Artifact");
}

static void test_intangible() {
    const DamageType types[] = {DamageType::Normal, DamageType::Thorns, DamageType::HpLoss};
    for (DamageType type : types) {
        Game g = combat();
        apply_power(g, player_target, player_target, PowerId::IntangiblePlayer, 2);
        apply_power(g, player_target, 0, PowerId::Intangible, 2);
        const int hp = g.player.creature.hp;
        g.player.creature.block = 4;
        DamageInfo info = hit(0, player_target, 30, type);
        damage(g, info);
        require(g.player.creature.hp == hp - (type == DamageType::HpLoss ? 1 : 0) &&
                g.player.creature.block == (type == DamageType::HpLoss ? 4 : 3),
                "IntangiblePlayer caps direct damage before consuming block, including HP_LOSS");
        require(calculate_damage(g, player_target, 0, 30, type) == 1,
                "monster Intangible final hook caps every calculated damage type");
        require(calculate_damage(g, 0, player_target, 30, type) == 1,
                "IntangiblePlayer final damage hook handles all damage types");
    }
    Game g = combat();
    apply_power(g, player_target, 0, PowerId::Intangible, 2);
    at_end_of_turn(g, 0);
    run_until_decision(g);
    require(power_amount(g.monsters[0].creature, PowerId::Intangible) == 2,
            "Intangible skips its first end-turn hook");
    at_end_of_turn(g, 0);
    run_until_decision(g);
    require(power_amount(g.monsters[0].creature, PowerId::Intangible) == 1,
            "Intangible decays on later end-turn hooks");
    at_end_of_round(g, 0);
    run_until_decision(g);
    require(power_amount(g.monsters[0].creature, PowerId::Intangible) == 1,
            "Intangible does not decay at end of round");
    at_end_of_turn(g, 0);
    run_until_decision(g);
    require(!has_power(g.monsters[0].creature, PowerId::Intangible), "Intangible removes its last stack");
    apply_power(g, player_target, player_target, PowerId::IntangiblePlayer, 1);
    at_end_of_turn(g, player_target);
    run_until_decision(g);
    require(has_power(g.player.creature, PowerId::IntangiblePlayer), "player Intangible persists through player end-turn");
    at_end_of_round(g, player_target);
    run_until_decision(g);
    require(!has_power(g.player.creature, PowerId::IntangiblePlayer), "player Intangible decays at end of round");
}

static void test_flight() {
    Game g = combat();
    apply_power(g, player_target, 0, PowerId::Flight, 2);
    apply_power(g, player_target, player_target, PowerId::Flight, 4);
    require(calculate_damage(g, player_target, 0, 9) == 4 &&
            calculate_damage(g, player_target, 0, 9, DamageType::Thorns) == 9 &&
            calculate_damage(g, player_target, 0, 9, DamageType::HpLoss) == 9,
            "Flight halves normal final damage and leaves Thorns and HP_LOSS intact");
    DamageInfo info = hit(player_target, 0, 3);
    damage(g, info);
    run_until_decision(g);
    require(power_amount(g.monsters[0].creature, PowerId::Flight) == 1,
            "surviving unblocked normal hit consumes one Flight stack");
    apply_power(g, player_target, 0, PowerId::Flight, 6);
    Power& p = g.monsters[0].creature.powers[find_power(g.monsters[0].creature, PowerId::Flight)];
    power_atStartOfTurn(g, p, 0);
    require(p.amount == 2 && power_amount(g.player.creature, PowerId::Flight) == 4,
            "Flight restores its constructor amount without sharing state across owners");
    g.monsters[0].creature.hp = 5;
    info.amount = 8;
    power_onAttacked(g, p, 0, info, 8);
    require(g.manager.actions.size == 1 && g.manager.actions[0].power == PowerId::Flight,
            "Flight willLive uses half of the post-block hit, even when the hit is lethal");
}

static void test_invincible() {
    Game g = combat();
    apply_power(g, player_target, 0, PowerId::Invincible, 5);
    apply_power(g, player_target, player_target, PowerId::Invincible, 11);
    const int hp = g.monsters[0].creature.hp;
    g.monsters[0].creature.block = 2;
    DamageInfo info = hit(player_target, 0, 9);
    damage(g, info);
    info.damage_type = DamageType::HpLoss;
    damage(g, info);
    require(g.monsters[0].creature.hp == hp - 5 && power_amount(g.monsters[0].creature, PowerId::Invincible) == 0,
            "Invincible limits cumulative post-block damage across damage types");
    apply_power(g, player_target, 0, PowerId::Invincible, 3);
    Power& p = g.monsters[0].creature.powers[find_power(g.monsters[0].creature, PowerId::Invincible)];
    power_atStartOfTurn(g, p, 0);
    require(p.amount == 5 && p.state.Invincible.maxAmt == 5 &&
            power_amount(g.player.creature, PowerId::Invincible) == 11,
            "Invincible restores each owner's constructor cap, not a later stack amount");
}

static void test_malleable() {
    const int owners[] = {player_target, 0};
    for (int owner : owners) {
        Game g = combat();
        const int source = owner == player_target ? 0 : player_target;
        Creature& c = get_creature(g, owner);
        const int hp = c.hp;
        apply_power(g, source, owner, PowerId::Malleable, 3);
        DamageInfo info = hit(source, owner, 2);
        add_bottom(g, info);
        damage(g, info);
        run_until_decision(g);
        require(c.hp == hp - (owner == player_target ? 2 : 4) &&
                c.block == (owner == player_target ? 1 : 7) &&
                power_amount(c, PowerId::Malleable) == (owner == player_target ? 4 : 5),
                "Malleable queues player block before the next hit, and monster block after it");
        apply_power(g, source, owner, PowerId::Malleable, 2);
        if (owner == player_target) at_end_of_round(g, owner);
        else at_end_of_turn(g, owner);
        require(power_amount(c, PowerId::Malleable) == 5,
                "Malleable resets at the owner's Java boundary and includes stacked base block");
    }
    Game g = combat();
    apply_power(g, player_target, 0, PowerId::Malleable, 3);
    DamageInfo info = hit(player_target, 0, 2, DamageType::Thorns);
    damage(g, info);
    require(g.manager.actions.empty(), "Malleable ignores Thorns");
    g.monsters[0].creature.hp = 2;
    info.damage_type = DamageType::Normal;
    damage(g, info);
    require(g.manager.actions.empty(), "Malleable does not grant block for a lethal hit");
}

static void test_block_and_constructor_time() {
    Game g = combat();
    queue_power(g, 0, player_target, PowerId::NoBlock, 2, true);
    g.manager.turn_has_ended = true;
    Effect e = g.manager.actions.pop_front();
    execute_effect(g, e);
    g.manager.turn_has_ended = false;
    at_end_of_round(g, player_target);
    run_until_decision(g);
    require(power_amount(g.player.creature, PowerId::NoBlock) == 1,
            "NoBlock constructor sees turnHasEnded before the action is resolved");
    require(calculate_block(g, 12) == 0, "NoBlock is the final card block modifier");
    apply_power(g, player_target, player_target, PowerId::NextTurnBlock, 8);
    Power& p = g.player.creature.powers[find_power(g.player.creature, PowerId::NextTurnBlock)];
    power_atStartOfTurn(g, p, player_target);
    run_until_decision(g);
    require(g.player.creature.block == 8 && !has_power(g.player.creature, PowerId::NextTurnBlock),
            "NextTurnBlock grants raw block before removing itself and bypasses card-only NoBlock");
    g.manager.turn_has_ended = true;
    queue_power(g, 0, player_target, PowerId::NoBlock, 3, true);
    e = g.manager.actions.pop_front();
    require(e.prepared_power.just_applied, "monster NoBlock captures an ended turn in its constructor");
    apply_power(g, player_target, 0, PowerId::Metallicize, 4);
    at_end_of_turn(g, 0);
    e = g.manager.actions.pop_front();
    execute_effect(g, e);
    require(g.monsters[0].creature.block == 4, "monster Metallicize receives the pre-end-turn-cards hook");
}

static void test_nightmare_and_sentinels() {
    Game g = combat();
    const int source = 0;
    Card& card = g.cards[source];
    card.base_block = 13;
    card.block = 0;
    card.cost_for_turn = 0;
    card.free_to_play_once = true;
    Power first{PowerId::Nightmare, 3};
    power_constructPower(g, first, player_target, player_target, 0, source);
    Power second{PowerId::Nightmare, 2};
    power_constructPower(g, second, player_target, player_target, 0, source);
    const int copy = first.state.Nightmare.card;
    require(copy != source && copy != second.state.Nightmare.card && g.cards[copy].uuid != card.uuid &&
            g.cards[copy].block == 13 && g.cards[copy].cost_for_turn == card.cost && g.cards[copy].free_to_play_once,
            "Nightmare creates a distinct stat-equivalent card and resets temporary attributes");
    card.base_block = 2;
    apply_prepared_power(g, player_target, player_target, first, 3);
    apply_prepared_power(g, player_target, player_target, second, 2);
    const Power& stored = g.player.creature.powers[find_power(g.player.creature, PowerId::Nightmare)];
    require(stored.amount == 3 && stored.state.Nightmare.card == copy && g.cards[copy].base_block == 13 &&
            g.player.creature.powers[find_power(g.player.creature, PowerId::Nightmare) + 1].amount == 2,
            "Nightmare applications retain separate powers and independent card objects");
    apply_power(g, player_target, 0, PowerId::Minion, 7);
    apply_power(g, player_target, 0, PowerId::PainfulStabs, 7);
    require(power_amount(g.monsters[0].creature, PowerId::Minion) == -1 &&
            power_amount(g.monsters[0].creature, PowerId::PainfulStabs) == -1,
            "Minion and Painful Stabs constructors use the inherited -1 amount");
}

int main() {
    test_focus();
    test_intangible();
    test_flight();
    test_invincible();
    test_malleable();
    test_block_and_constructor_time();
    test_nightmare_and_sentinels();
    std::puts("Power batch 02 Java-derived cases passed (not oracle trace acceptance)");
}
