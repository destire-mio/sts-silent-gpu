#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "Action batch 02: %s\n", message); std::exit(1); }
}

static Game combat() {
    Game g{};
    reset(g, 42);
    CombatSetup setup{};
    start_combat(g, setup);
    return g;
}

static int card(Game& g, CardId id, Pile& pile, int upgrades = 0) {
    const int index = g.cards.size;
    Card c = make_card(id, upgrades);
    c.uuid = index + 1;
    g.cards.push_back(c);
    pile.push_back(index);
    return index;
}

static Effect effect(EffectKind kind) {
    Effect e{};
    e.kind = kind;
    action_initialize_state(e);
    return e;
}

static void temp_cards() {
    Game g = combat();
    const int source = card(g, CardId::Strike_G, g.limbo, 1);
    Effect both = effect(EffectKind::MakeTempCardInDiscardAndDeck);
    both.card = source;
    const int rng = g.rng.card_random.counter;
    execute_effect(g, both);
    require(g.draw.size == 1 && g.discard.size == 1 && g.rng.card_random.counter == rng,
            "empty draw insertion consumes no RNG");
    require(g.draw[0] != g.discard[0] && g.cards[g.draw[0]].uuid != g.cards[g.discard[0]].uuid &&
            g.cards[g.draw[0]].base_damage == 9, "two independent stat-equivalent copies");
    Effect draw = effect(EffectKind::MakeTempCardInDrawPile);
    draw.card = source;
    draw.amount = 6;
    draw.state.MakeTempCardInDrawPile.random_spot = true;
    draw.state.MakeTempCardInDrawPile.to_bottom = true;
    const int top = g.draw[0];
    execute_effect(g, draw);
    require(g.draw.size == 7 && g.draw[6] == top && g.rng.card_random.counter == rng,
            "bottom insertion takes precedence over random insertion for six cards");
    draw = effect(EffectKind::MakeTempCardInDrawPile);
    draw.card = source;
    draw.amount = 6;
    draw.state.MakeTempCardInDrawPile.random_spot = true;
    execute_effect(g, draw);
    require(g.draw.size == 13 && g.rng.card_random.counter == rng + 6,
            "random draw insertion uses cardRandomRng for every nonempty pile");
    for (int i = 0; i < 9; ++i) card(g, CardId::Defend_G, g.hand);
    Effect hand = effect(EffectKind::MakeTempCardInHand);
    hand.card = source;
    hand.amount = 3;
    hand.state.MakeTempCardInHand.same_uuid = true;
    execute_effect(g, hand);
    require(g.hand.size == 10 && g.discard.size == 3 && g.cards[g.hand[9]].uuid == g.cards[source].uuid &&
            g.cards[g.discard[2]].uuid == g.cards[source].uuid,
            "same UUID survives both hand insertion and overflow to discard");
    require(g.manager.total_discarded_this_turn == 0 && g.rng.card_random.counter == rng + 6,
            "temporary-card overflow is not a manual discard or random card operation");
}

static void modify_damage() {
    Game g = combat();
    const int a = card(g, CardId::Strike_G, g.hand);
    const int b = card(g, CardId::Strike_G, g.draw);
    const int c = card(g, CardId::Strike_G, g.limbo);
    const int other = card(g, CardId::Strike_G, g.exhaust);
    g.cards[b].uuid = g.cards[a].uuid;
    g.cards[c].uuid = g.cards[a].uuid;
    g.player.card_in_use = a;
    const int inactive = g.cards.size;
    g.cards.push_back(g.cards[a]);
    Effect e = effect(EffectKind::ModifyDamage);
    e.amount = -8;
    e.state.ModifyDamage.uuid = g.cards[a].uuid;
    execute_effect(g, e);
    require(g.cards[a].base_damage == 0 && g.cards[b].base_damage == 0 && g.cards[c].base_damage == 0,
            "UUID modification reaches hand, draw and limbo and clamps base damage");
    require(g.cards[other].base_damage == 6 && g.cards[inactive].base_damage == 6 && g.cards[a].damage == -1,
            "same card ID and inactive UUID copies are untouched; calculated damage is not recomputed");
    e = effect(EffectKind::ModifyDamage);
    e.amount = 2;
    e.state.ModifyDamage.uuid = g.cards[a].uuid;
    execute_effect(g, e);
    require(g.cards[a].base_damage == 2 && g.cards[b].base_damage == 2,
            "the card-in-use alias is modified once, as in the Java identity set");
}

static void put_on_deck() {
    Game g = combat();
    for (int i = 0; i < 5; ++i) card(g, CardId::Strike_G, g.hand);
    Effect e = effect(EffectKind::PutOnDeck);
    e.amount = 8;
    const int rng = g.rng.card_random.counter;
    execute_effect(g, e);
    require(g.hand.size == 2 && g.draw.size == 3 && e.amount == 5 && g.rng.card_random.counter == rng + 3,
            "nonrandom all-hand branch preserves Java's shrinking-loop behavior and RNG");
    g = combat();
    const int a = card(g, CardId::Strike_G, g.hand);
    card(g, CardId::Defend_G, g.hand);
    card(g, CardId::Survivor, g.hand);
    g.manager.card_queue.push_back({a, 0});
    g.manager.card_queue.push_back({a, 0});
    e = effect(EffectKind::PutOnDeck);
    e.amount = 1;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.screen == Screen::Discard && g.put_on_deck_num == 1, "requested hand selection suspends the action");
    step(g, {ActionKind::Choose, 0});
    require(g.selected_cards[0] == a && g.draw.empty(), "selection does not move a card before confirmation");
    g.manager.card_queue.clear();
    step(g, {ActionKind::Proceed});
    require(g.draw.size == 1 && g.draw[0] == a && g.hand.size == 2 && g.discard.empty() &&
            g.selected_cards_retrieved && g.manager.total_discarded_this_turn == 0,
            "confirmed placement preserves card identity and does not discard");
    g.manager.card_queue.push_back({a, 0});
    g.manager.card_queue.push_back({a, 0});
    remove_queued_card(g, a);
    require(g.manager.card_queue.size == 1, "CardGroup movement removes only the first queued occurrence");
}

static void upgrades_and_cost() {
    Game g = combat();
    const int a = card(g, CardId::Strike_G, g.hand);
    card(g, CardId::Dazed, g.hand);
    Effect random = effect(EffectKind::UpgradeRandomCard);
    const int rng = g.rng.shuffle.counter;
    execute_effect(g, random);
    require(g.cards[a].upgrades == 1 && g.rng.shuffle.counter == rng + 1,
            "one eligible upgrade still shuffles with shuffleRng");
    random = effect(EffectKind::UpgradeRandomCard);
    execute_effect(g, random);
    require(g.rng.shuffle.counter == rng + 1, "no eligible cards consume no shuffle RNG");
    const int d = card(g, CardId::Defend_G, g.draw);
    const int s = card(g, CardId::Survivor, g.discard);
    const int n = card(g, CardId::Neutralize, g.exhaust);
    const int l = card(g, CardId::Strike_G, g.limbo);
    Effect all = effect(EffectKind::Apotheosis);
    execute_effect(g, all);
    require(g.cards[d].upgrades == 1 && g.cards[s].upgrades == 1 && g.cards[n].upgrades == 1 &&
            g.cards[l].upgrades == 0, "Apotheosis covers four combat piles and excludes limbo");
    g.cards[a].cost_for_turn = 4;
    g.cards[a].cost = 2;
    g.cards[l].cost_for_turn = -1;
    g.hand.push_back(l);
    Effect bullet = effect(EffectKind::ApplyBulletTime);
    execute_effect(g, bullet);
    require(g.cards[a].cost == 2 && g.cards[a].cost_for_turn == 0 &&
            g.cards[l].cost_for_turn == -1 && g.cards[g.hand[1]].cost_for_turn == -2,
            "Bullet Time sets nonnegative turn costs to zero without changing X or unplayable costs");
}

static void armaments() {
    Game g = combat();
    const int curse = card(g, CardId::AscendersBane, g.hand);
    const int a = card(g, CardId::Strike_G, g.hand);
    const int b = card(g, CardId::Defend_G, g.hand);
    Effect e = effect(EffectKind::Armaments);
    add_bottom(g, e);
    run_until_decision(g);
    require(g.screen == Screen::Discard && g.selection_for_upgrade && g.hand.size == 2 &&
            g.manager.current_action.state.Armaments.cannot_upgrade[0] == curse,
            "Armaments stores excluded cards on its own suspended action");
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Proceed});
    require(g.hand.size == 3 && g.hand[0] == b && g.hand[1] == a && g.hand[2] == curse &&
            g.cards[a].upgrades == 1 && g.cards[b].upgrades == 0 && g.selected_cards.empty(),
            "Armaments returns the chosen card then excluded cards in Java order");
    e = effect(EffectKind::Armaments);
    e.state.Armaments.upgraded = true;
    execute_effect(g, e);
    require(g.cards[b].upgrades == 1 && g.cards[curse].upgrades == 0, "Armaments+ upgrades eligible hand cards");
}

static void attack_from_deck() {
    Game g = combat();
    const int a = card(g, CardId::Strike_G, g.draw);
    const int b = card(g, CardId::Neutralize, g.draw);
    const int skill = card(g, CardId::Defend_G, g.draw);
    Effect e = effect(EffectKind::AttackFromDeckToHand);
    e.amount = 2;
    const int rng = g.rng.card_random.counter;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.screen == Screen::Grid && g.grid_cards.size == 2 && g.rng.card_random.counter == rng + 1,
            "attack candidate insertion consumes RNG after the first candidate");
    step(g, {ActionKind::Choose, 0});
    require(g.screen == Screen::Grid && g.draw.size == 3, "draw pile retains selected identity while grid is open");
    step(g, {ActionKind::Choose, 0});
    require(g.selected_cards.empty(), "grid selection permits toggling a selected card");
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Choose, 1});
    require(g.screen == Screen::Combat && g.draw.size == 1 && g.draw[0] == skill && g.hand.size == 2 &&
            ((g.hand[0] == a && g.hand[1] == b) || (g.hand[0] == b && g.hand[1] == a)),
            "grid confirmation moves the two original attacks in selection order");
    for (int i = 0; i < 8; ++i) card(g, CardId::Defend_G, g.hand);
    const int overflow = card(g, CardId::Strike_G, g.draw);
    g.cards[overflow].damage = 99;
    e = effect(EffectKind::AttackFromDeckToHand);
    e.amount = 1;
    execute_effect(g, e);
    require(g.hand.size == 10 && g.discard[0] == overflow && g.cards[overflow].damage == 99 &&
            g.manager.total_discarded_this_turn == 0, "full-hand deck extraction preserves the card and is not manual discard");
}

static void moves_and_removal() {
    Game g = combat();
    Effect set = effect(EffectKind::SetMove);
    set.target = 0;
    set.card = 7;
    set.amount = 2;
    set.extra = static_cast<int>(Intent::Attack);
    set.state.SetMove.the_multiplier = 1;
    set.state.SetMove.is_multiplier = true;
    std::strcpy(set.state.SetMove.the_next_name, "test move");
    set.state.SetMove.has_name = true;
    execute_effect(g, set);
    create_monster_intent(g, 0);
    require(g.monsters[0].pending_hits == 1 && g.monsters[0].intent_hits == 1 && g.monsters[0].intent_multi,
            "SetMove preserves an explicit multiplier of one");
    Effect show = effect(EffectKind::ShowMoveName);
    show.source = 0;
    add_bottom(g, show);
    require(!g.monsters[0].move_name_present && g.manager.actions[0].state.ShowMoveName.has_msg &&
            std::strcmp(g.manager.actions[0].state.ShowMoveName.msg, "test move") == 0,
            "default ShowMoveName captures and clears the monster name during construction");
    g.manager.actions.clear();
    apply_power(g, player_target, player_target, PowerId::Strength, 2);
    const Power old = g.player.creature.powers[0];
    const Effect remove = units::actions::RemovePower::from_instance(player_target, player_target, old);
    add_bottom(g, remove);
    apply_power(g, player_target, player_target, PowerId::Strength, 4);
    remove_power(g, player_target, PowerId::Strength);
    require(g.manager.actions[0].prepared_power.amount == 6,
            "queued power references retain the object's final amount after removal");
    g.manager.actions.clear();
    apply_power(g, player_target, player_target, PowerId::Strength, 3);
    Effect queued = remove;
    execute_effect(g, queued);
    require(power_amount(g.player.creature, PowerId::Strength) == 3,
            "removing a stale power instance leaves a replacement with the same ID");
    queued = units::actions::RemovePower::from_instance(player_target, player_target, g.player.creature.powers[0]);
    apply_power(g, player_target, player_target, PowerId::Dexterity, 2);
    execute_effect(g, queued);
    require(!has_power(g.player.creature, PowerId::Strength) && has_power(g.player.creature, PowerId::Dexterity),
            "instance removal keeps other powers after list insertion");
}

static void remaining_actions() {
    Game g = combat();
    const int a = card(g, CardId::Strike_G, g.hand);
    Effect dont = effect(EffectKind::SetDontTrigger);
    dont.card = a;
    dont.flag = true;
    execute_effect(g, dont);
    g.manager.card_queue.push_back({a, 0, 3});
    run_until_decision(g);
    require(g.player.cards_played_this_turn == 0 && g.manager.played_cards.size == 0 &&
            !g.cards[a].dont_trigger_on_use_card && g.discard[0] == a,
            "suppressed use skips histories and resets suppression after card movement");
    Effect shuffle = effect(EffectKind::ShuffleGroup);
    shuffle.state.ShuffleGroup.group = PileId::Draw;
    const int rng = g.rng.shuffle.counter;
    execute_effect(g, shuffle);
    require(g.rng.shuffle.counter == rng + 1 && g.discard.size == 1 && g.draw.empty(),
            "ShuffleAction shuffles its target even when empty without moving discard cards");
    const int status = card(g, CardId::Slimed, g.limbo);
    Effect obtain = effect(EffectKind::AddCardToDeck);
    obtain.card = status;
    execute_effect(g, obtain);
    obtain = effect(EffectKind::AddCardToDeck);
    obtain.card = a;
    execute_effect(g, obtain);
    require(g.run.master_deck.size == 2 && g.obtained_cards[static_cast<int>(CardId::Slimed)] == 1 &&
            g.obtained_cards[static_cast<int>(CardId::Strike_G)] == 0,
            "AddCardToDeck tracks common acquisition and excludes basic rarity");
    g.monsters[0].creature.gold = 20;
    Effect suicide = effect(EffectKind::Suicide);
    suicide.target = 0;
    add_bottom(g, suicide);
    run_until_decision(g);
    require(g.monsters[0].creature.hp == 0 && g.monsters[0].creature.gold == 0 && g.screen == Screen::Victory,
            "Suicide clears stolen gold and settles combat death");
}

int main() {
    temp_cards(); modify_damage(); put_on_deck(); upgrades_and_cost();
    armaments(); attack_from_deck(); moves_and_removal(); remaining_actions();
    std::puts("Action batch 02 source-derived cases passed (not oracle acceptance)");
}
