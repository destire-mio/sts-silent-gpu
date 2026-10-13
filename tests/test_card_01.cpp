#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Card mismatch: %s\n", message);
        std::exit(1);
    }
}

int main() {
    Game g{};
    reset(g, 101);
    CombatSetup setup{};
    setup.encounter = EncounterId::TwoLouse;
    setup.deck.push_back(make_card(CardId::DramaticEntrance));
    start_combat(g, setup);
    const int first = g.monster_order[0];
    const int second = g.monster_order[1];
    apply_power(g, player_target, first, PowerId::Vulnerable, 1);
    const int first_hp = g.monsters[first].creature.hp;
    const int second_hp = g.monsters[second].creature.hp;
    calculate_card_damage(g, 0, second);
    require(g.cards[0].multi_damage_count == 2 && g.multi_damage[0][0] == 12 && g.multi_damage[0][1] == 8,
            "an explicit autoplay target preserves damage calculation for each enemy");
    Effect hit = units::actions::DamageAllEnemies::from_card(g, 0);
    execute_effect(g, hit);
    require(g.monsters[first].creature.hp == first_hp - 12 && g.monsters[second].creature.hp == second_hp - 8,
            "the group hit applies each enemy's vulnerability result");
    std::puts("Java-derived group damage case passed (not oracle trace acceptance)");
}
