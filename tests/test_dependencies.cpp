#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;
using namespace spire::units::actions;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
static Game combat() {
    Game g{};
    reset(g, 42);
    CombatSetup setup{};
    for (int i = 0; i < 5; ++i) setup.deck.push_back(make_card(CardId::Strike_G));
    start_combat(g, setup);
    g.monsters[0].creature.hp = g.monsters[0].creature.max_hp = 200;
    return g;
}
static void poison_chain() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::Envenom, 2);
    apply_power(g, player_target, player_target, PowerId::NoxiousFumes, 3);
    step(g, {ActionKind::Play, 0, 0});
    require(g.monsters[0].creature.hp == 194 && power_amount(g.monsters[0].creature, PowerId::Poison) == 2,
            "Envenom applies Poison after unblocked normal damage");
    step(g, {ActionKind::EndTurn, -1, -1});
    require(g.monsters[0].creature.hp == 192 && power_amount(g.monsters[0].creature, PowerId::Poison) == 4,
            "Poison ticks and loses one stack before Noxious Fumes reapplies it after draw");
}
static void stasis_returns_identity() {
    Game g = combat();
    g.monsters.push_back(construct_monster(g, MonsterId::Cultist));
    g.monster_order.push_back(1);
    const int original = g.hand.pop_back();
    g.draw.push_back(original);
    Effect steal{};
    steal.kind = EffectKind::ApplyStasisAction;
    steal.target = 0;
    add_bottom(g, steal);
    run_until_decision(g);
    require(g.draw.empty() && g.limbo.empty() && g.hand.size == 4, "Stasis removes the stolen card through ShowCard");
    DamageInfo hit{};
    hit.source = player_target;
    hit.target = 0;
    hit.amount = hit.info_base = 200;
    hit.damage_type = DamageType::Thorns;
    damage(g, hit);
    run_until_decision(g);
    require(g.hand.size == 5 && g.hand[4] != original && g.cards[g.hand[4]].uuid == g.cards[original].uuid,
            "Stasis returns a distinct card object sharing the stolen card UUID after death");
}
static void time_warp_and_heal() {
    Game g = combat();
    g.cards[g.hand[0]] = make_card(CardId::Defend_G);
    const int card = g.hand[0];
    g.cards[card].return_to_hand = true;
    g.cards[card].cost_for_turn = 0;
    apply_power(g, 0, 0, PowerId::TimeWarp, 0);
    for (int i = 0; i < 12; ++i) {
        int hand = 0;
        while (g.hand[hand] != card) ++hand;
        step(g, {ActionKind::Play, hand, -1});
    }
    require(g.turn == 2 && power_amount(g.monsters[0].creature, PowerId::Strength) == 2,
            "The twelfth play forces the monster turn and grants Time Warp Strength");
    g = combat();
    g.player.creature.hp = 20;
    g.monsters[0].creature.hp = 3;
    DamageInfo hit{};
    hit.source = player_target;
    hit.amount = hit.info_base = 10;
    add_bottom(g, VampireDamageAction::construct(0, hit));
    run_until_decision(g);
    require(g.player.creature.hp == 23, "Vampire healing survives combat cleanup and uses actual HP lost");
}
static void rear_attack_order() {
    Game g = combat();
    g.monsters[0].creature.draw_x = 100.0f;
    apply_power(g, player_target, player_target, PowerId::Surrounded, -1);
    apply_power(g, player_target, player_target, PowerId::IntangiblePlayer, 1);
    monster_apply_powers(g, 0);
    run_until_decision(g);
    require(has_power(g.monsters[0].creature, PowerId::BackAttack), "Surrounded attaches the rear attacker power");
    require(calculate_intent_damage(g, 0, 10) == 1, "Intent applies the rear bonus before final damage reductions");
    step(g, {ActionKind::Play, 0, 0});
    require(g.player.creature.flip_horizontal && rear_damage_multiplier(g, 0) == 1.0f,
            "Targeting the rear monster changes facing before its next damage calculation");
}
int main() {
    poison_chain();
    stasis_returns_identity();
    time_warp_and_heal();
    rear_attack_order();
}
