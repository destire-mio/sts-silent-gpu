#include "core/engine.hpp"

using namespace spire;

static Game combat(int64_t seed, CardId card, EncounterId encounter) {
    Game game{};
    reset(game, seed);
    CombatSetup setup{};
    setup.deck.push_back(make_card(card));
    setup.hp = 1;
    setup.max_hp = 66;
    setup.encounter = encounter;
    start_combat(game, setup);
    return game;
}

int main() {
    Game spray = combat(1000001178, CardId::DaggerSpray, EncounterId::FourShapes);
    step(spray, {ActionKind::Play, 0, -1});
    SPIRE_ASSERT(spray.screen == Screen::Death);
    SPIRE_ASSERT(spray.manager.actions.size == 3);
    constexpr EffectKind spray_queue[]{EffectKind::VFX, EffectKind::DamageAllEnemies, EffectKind::UseCard};
    for (int i = 0; i < 3; ++i)
        SPIRE_ASSERT(spray.action_objects[spray.manager.actions[i]].kind == spray_queue[i]);
    SPIRE_ASSERT(spray.action_objects[spray.manager.actions[0]].source == no_target);

    Game plant = combat(1000000067, CardId::Defend_G, EncounterId::SnakePlant);
    step(plant, {ActionKind::EndTurn, -1, -1});
    SPIRE_ASSERT(plant.screen == Screen::Death);
    SPIRE_ASSERT(plant.manager.actions.size == 6);
    constexpr EffectKind plant_queue[]{EffectKind::VFX, EffectKind::Damage, EffectKind::VFX, EffectKind::Damage, EffectKind::RollMove, EffectKind::Wait};
    for (int i = 0; i < 6; ++i)
        SPIRE_ASSERT(plant.action_objects[plant.manager.actions[i]].kind == plant_queue[i]);
}
