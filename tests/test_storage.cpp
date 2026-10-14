#include "host/batch.hpp"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

static void compact_storage() {
    Game game{};
    reset(game, 3);
    CombatSetup setup{};
    for (int i = 0; i < master_deck_capacity; ++i) setup.deck.push_back(make_card(CardId::Defend_G));
    start_combat(game, setup);
    require(game.cards.size == 128 && game.cards[127].uuid == 128, "last byte-sized card index retains its UUID");
    for (int round = 0; round < 8; ++round) {
        for (int i = 0; i < action_capacity; ++i)
            add_bottom(game, units::actions::GainEnergy::make(1));
        run_until_decision(game);
    }
    require(game.player.energy == 3 + 8 * action_capacity, "completed action slots are reusable");
    for (int round = 0; round < 2; ++round) {
        for (int i = 0; i < action_capacity; ++i)
            add_bottom(game, units::actions::GainEnergy::make(1));
        clear_actions(game, game.manager.actions);
    }
    require(game.player.energy == 3 + 8 * action_capacity, "discarded queued actions release slots without executing");
    for (int i = 0; i < card_capacity; ++i) game.multi_damage.base[card_capacity - 1 - i] = i + 1;
    const int amounts[]{1, 200000, 1, 300000};
    DamageArray row{};
    row.assign(game.damage_exceptions, amounts, 4);
    game.multi_damage.set(0, row);
    apply_power(game, player_target, player_target, PowerId::Strength, 4);
    const int text = game.text.intern("CLOSED");
    Game copy = game;
    copy.multi_damage.base[card_capacity - 1] = 99;
    DamageArray changed = copy.multi_damage.get(0);
    const int replacement[]{9, 8, 7, 6};
    changed.assign(copy.damage_exceptions, replacement, 4);
    copy.multi_damage.set(0, changed);
    require(game.multi_damage.get(0).at(game.damage_exceptions, 1) == 200000 &&
            copy.multi_damage.get(0).at(copy.damage_exceptions, 1) == 8,
            "relocating and replacing sparse damage preserves 32-bit values and isolates storage");
    apply_power(copy, player_target, player_target, PowerId::Strength, 2);
    require(game.multi_damage.get(card_capacity - 1).at(game.damage_exceptions, 0) == 1 && power_amount(game, game.player.creature, PowerId::Strength) == 4 &&
            power_amount(copy, copy.player.creature, PowerId::Strength) == 6 &&
            std::strcmp(copy.text[text], "CLOSED") == 0, "a relocated Game owns independent arena and text storage");
}

static void damage_info_lifetimes() {
    Game game{};
    reset(game, 3);
    CombatSetup setup{};
    setup.deck = game.run.master_deck;
    start_combat(game, setup);
    alignas(Effect) unsigned char storage[sizeof(Effect)];
    std::memset(storage, 0xa5, sizeof(storage));
    Effect& hit = *new (storage) Effect{};
    hit.kind = EffectKind::Damage;
    hit.target = 0;
    hit.amount = hit.info_base = 9;
    const int enemy_hp = game.monsters[0].creature.hp;
    add_bottom(game, hit);
    run_until_decision(game);
    require(game.monsters[0].creature.hp == enemy_hp - 9,
            "a fresh damage action must not inherit a reference from reused storage");

    Effect attack = units::actions::Damage::from_shared_info(
        units::actions::Damage::monster_info(game, 0, 0), player_target,
        units::actions::Damage::monster_identity(0));
    const int original_amount = attack.amount;
    apply_power(game, 0, 0, PowerId::Strength, 3);
    const int hp = game.player.creature.hp;
    units::actions::Damage::execute(game, attack);
    require(game.player.creature.hp == hp - original_amount - 3 && attack.amount == original_amount,
            "DamageAction reads its live DamageInfo while retaining its constructor amount");
}

static void interleaved_histories() {
    MoveHistories storage{};
    MoveHistory histories[7]{};
    for (int i = 0; i < 1024; ++i) storage.append(histories[i % 7], i % 121 - 60);
    MoveHistories copy = storage;
    for (int owner = 0; owner < 7; ++owner) {
        const int last = 1023 - (1023 - owner) % 7;
        for (int back = 0; back < histories[owner].size; ++back)
            require(copy.last(histories[owner], back) == (last - back * 7) % 121 - 60,
                    "packed links preserve interleaved histories through the final arena slot");
    }
}

static void relocated_orbs() {
    Game game{};
    for (int i = 0; i < 128; ++i) {
        const int orb = game.orbs.add(static_cast<OrbKind>(i % 4), 100000 + i, 200000 + i);
        game.orbs.channeled.push_back(orb);
    }
    game.orbs.slots.push_back(127);
    game.orbs.slots.push_back(-1);
    game.orbs.slots.push_back(-2);
    Game copy = game;
    copy.orbs.objects[127].evoke += 300000;
    for (int i = 0; i < 128; ++i)
        require(copy.orbs.kind(copy.orbs.channeled[i]) == static_cast<OrbKind>(i % 4) &&
                copy.orbs.objects[i].passive == 100000 + i,
                "relocation preserves distinct orb types, history references and 32-bit amounts");
    require(copy.orbs.slots[0] == 127 && copy.orbs.slots[1] == -1 && copy.orbs.slots[2] == -2 &&
            game.orbs.objects[127].evoke == 200127 && copy.orbs.objects[127].evoke == 500127,
            "the final orb index and both empty-slot identities survive copying without shared storage");
}

static void fault_isolation() {
    host::Batch batch(2);
    batch.reset({7, 8});
    for (Game& game : batch.games) {
        CombatSetup setup{};
        setup.deck = game.run.master_deck;
        start_combat(game, setup);
    }
    record_fault(batch.games[0].fault, 123);
    record_fault(batch.games[0].fault, 456);
    unsigned char before[sizeof(Game)];
    std::memcpy(before, &batch.games[0], sizeof(Game));
    const int turn = batch.games[1].turn;
    batch.step({{ActionKind::EndTurn}, {ActionKind::EndTurn}});
    require(batch.faults() == std::vector<uint32_t>({123, 0}) &&
            std::memcmp(before, &batch.games[0], sizeof(Game)) == 0 && batch.games[1].turn == turn + 1,
            "the first fault persists, the faulted environment does not step, and its neighbor steps");
    std::memcpy(before, &batch.games[1], sizeof(Game));
    batch.reset_at(0, 9);
    require(batch.faults() == std::vector<uint32_t>({0, 0}) && batch.games[0].run.seed == 9 &&
            std::memcmp(before, &batch.games[1], sizeof(Game)) == 0, "indexed reset clears the fault without resetting its neighbor");
}

static void card_value_lifetimes() {
    Game game{};
    reset(game, 3);
    const int card = game.cards.size;
    game.cards.add(make_card(CardId::MasterfulStab));
    for (int i = 0; i < 200; ++i) card_tookDamage(game, card);
    require(game.cards[card].cost == 200 && game.cards[card].cost_for_turn == 200,
            "repeated HP loss must not wrap a card cost at byte range");
    game.cards[card].priced = true;
    game.cards[card].price = 75;
    game.cards[card].bottle_flame = true;
    Card copy = card_makeStatEquivalentCopy(game, card);
    require(!copy.priced && copy.energy_on_use == 0 && copy.bottle_flame && copy.cost == 200,
            "stat-equivalent copies discard shop price and preserve bottled identity flags and cost");
    require(upgrade_name_order(1) < upgrade_name_order(10) &&
            upgrade_name_order(10) < upgrade_name_order(2) &&
            upgrade_name_order(2) < upgrade_name_order(65535),
            "repeated upgrade suffixes sort as decimal strings");
}

static void host_overflow_aborts() {
    const pid_t child = fork();
    require(child >= 0, "fork");
    if (child == 0) {
        const rlimit limit{0, 0};
        setrlimit(RLIMIT_CORE, &limit);
        Game game{};
        for (int i = 0; i <= card_capacity; ++i) game.cards.add(Card{});
        _exit(0);
    }
    int status = 0;
    require(waitpid(child, &status, 0) == child && WIFSIGNALED(status) &&
            (WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP), "host capacity overflow must abort in release builds");
}

static void card_reference_lifetimes() {
    Game game{};
    const int garbage = create_card(game, make_card(CardId::Shiv));
    const int values[]{1, 9, 1};
    DamageArray damage{};
    damage.assign(game.damage_exceptions, values, 3);
    game.multi_damage.set(garbage, damage);
    const auto add = [&]() { return create_card(game, make_card(CardId::Defend_G)); };
    game.hand.push_back(add()); game.draw.push_back(add()); game.discard.push_back(add());
    game.exhaust.push_back(add()); game.limbo.push_back(add()); game.drawn_cards.push_back(add());
    game.selection_excluded.push_back(add()); game.pending_obtains.push_back(add());
    Effect exhume{}; exhume.kind = EffectKind::Exhume; exhume.payload.state.Exhume.selecting = true;
    game.action_objects.add(exhume);
    const int detached = add();
    game.detached_master_cards.push_back(detached);
    Relic bottle{RelicId::BottledFlame}; bottle.state.BottledFlame.card = game.cards[detached].uuid;
    game.run.relics.push_back(bottle);
    game.reward_cards.push_back(add());
    game.player.card_in_use = add(); game.discovery_card = add();
    game.manager.played_cards.record(add()); game.manager.card_queue.push_back({add()});
    mark_card_move(game, add());
    game.screen = Screen::Grid;
    game.grid_cards.push_back(add()); game.selected_cards.push_back(add());
    RoomReward reward{}; reward.cards.push_back(add()); game.reward_objects.push_back(reward);
    game.room_kind = RoomKind::Shop;
    game.shop.colored.push_back(add()); game.shop.colorless.push_back(add());
    Power nightmare{PowerId::Nightmare, 1}; nightmare.state.Nightmare.card = add();
    game.player.creature.powers.push_back(game.power_objects, game.power_objects.add(nightmare));
    Power stasis{PowerId::Stasis, -1}; stasis.state.Stasis.card = add();
    game.action_objects.add(units::actions::ApplyPower::make(player_target, player_target, stasis, 1));
    Power removed{PowerId::Stasis, -1}; removed.instance = 19; removed.state.Stasis.card = add();
    game.power_objects.add(removed);
    game.action_objects.add(units::actions::RemovePower::from_instance(player_target, player_target, removed));
    Monster monster{};
    Power captured{PowerId::Stasis, -1}; captured.state.Stasis.card = add();
    monster.creature.powers.push_back(game.power_objects, game.power_objects.add(captured));
    game.monsters.push_back(monster);
    game.monster_order.push_back(0);
    Effect held{}; held.kind = EffectKind::ShowCard; held.card = add();
    hold_action(game, held);
    Effect armaments{}; armaments.kind = EffectKind::Armaments;
    armaments.payload.state.Armaments.cannot_upgrade_count = 1;
    armaments.payload.state.Armaments.cannot_upgrade[0] = add();
    game.action_objects.add(armaments);
    Effect dual{}; dual.kind = EffectKind::DualWield;
    dual.payload.state.DualWield.count = 1; dual.payload.state.DualWield.cannotDuplicate[0] = add();
    game.action_objects.add(dual);
    Effect choose{}; choose.kind = EffectKind::ChooseOne;
    choose.payload.state.ChooseOne.count = 1; choose.payload.state.ChooseOne.choices[0] = add();
    game.action_objects.add(choose);
    Effect apply{}; apply.kind = EffectKind::ApplyStasisAction; apply.payload.state.ApplyStasisAction.card = add();
    game.action_objects.add(apply);
    game.event.id = EventId::GremlinMatchGame;
    auto& match = game.event.state.GremlinMatchGame;
    match.chosen = add(); match.count = 1; match.cards[0] = add();
    match.cardsMatched = 1; match.matched[0] = add();
    const int count = game.cards.count();
    const int next_uuid = game.next_card_uuid;
    collect_cards(game);
    require(game.cards.count() == count - 1 && !game.cards.contains(garbage), "only the unreferenced card is reclaimed");
    for (int i = 1; i < count; ++i) require(game.cards.contains(i), "a card held only by one root survives");
    require(game.cards.values[garbage].id == CardId::Count && game.cards.values[garbage].damage == static_cast<int>(0xa5a5a5a5u),
            "released card bytes are poisoned");
    require(game.multi_damage.head[garbage] == 0, "releasing a card releases its damage exceptions");
    for (auto word : game.damage_exceptions.used) require(word == 0, "unreferenced damage rows do not leak");
    const int reused = create_card(game, make_card(CardId::Strike_G));
    require(reused == garbage && game.cards[reused].uuid == next_uuid, "a reused slot receives the next UUID");
    for (int i = 0; i < 2000; ++i) {
        collect_cards(game);
        require(create_card(game, make_card(CardId::Shiv)) == reused, "cumulative creation does not consume live-card capacity");
    }
    Game copy = game;
    collect_cards(copy);
    require(game.cards.contains(reused) && !copy.cards.contains(reused), "copied arenas own independent free lists");
    copy.player.creature.powers.clear(); copy.run.relics.clear();
    collect_cards(copy);
    require(!copy.cards.contains(nightmare.state.Nightmare.card) && !copy.cards.contains(detached),
            "detached powers and expired master-reference bookkeeping do not retain cards");
    copy.monster_order.clear();
    collect_cards(copy);
    require(!copy.cards.contains(captured.state.Stasis.card), "an expired room's monster arena is not a card root");

    Game event{};
    event.event.id = EventId::Bonfire;
    event.event.state.Bonfire.offeredCard = create_card(event, make_card(CardId::Strike_G)) + 1;
    collect_cards(event);
    require(event.cards.count() == 1, "Bonfire retains the offered card");
    event.event = Event{}; event.event.id = EventId::Sssserpent;
    event.event.state.Sssserpent.curse = create_card(event, make_card(CardId::Doubt));
    collect_cards(event);
    require(event.cards.count() == 1 && event.cards[event.event.state.Sssserpent.curse].id == CardId::Doubt,
            "Sssserpent retains its curse after the previous event releases its card");
}

static void reused_slots_preserve_instance_rules() {
    Game game{};
    const int first = create_card(game, make_card(CardId::Strike_G));
    const int garbage = create_card(game, make_card(CardId::Shiv));
    const int second = copy_card(game, first, true);
    const int historical = copy_card(game, first, true);
    game.player.card_in_use = first;
    game.hand.push_back(first); game.draw.push_back(second);
    game.manager.played_cards.record(historical);
    collect_cards(game);
    Effect modify{}; modify.kind = EffectKind::ModifyBlock; modify.amount = 3;
    modify.payload.state.ModifyBlock.uuid = game.cards[first].uuid;
    units::actions::ModifyBlock::execute(game, modify);
    require(game.cards[first].base_block_adjust == 3 && game.cards[second].base_block_adjust == 3 &&
            game.cards[historical].base_block_adjust == 0,
            "UUID updates deduplicate battle instances and exclude history-only cards across an empty slot");
    const int reused = create_card(game, make_card(CardId::Defend_G));
    game.hand.push_back(reused);
    units::actions::ModifyBlock::execute(game, modify);
    require(reused == garbage && game.cards[first].base_block_adjust == 6 && game.cards[second].base_block_adjust == 6 &&
            game.cards[reused].base_block_adjust == 0 && game.cards[historical].base_block_adjust == 0,
            "reusing an earlier slot does not change UUID membership or apply a rule twice");
}

int main() {
    compact_storage();
    damage_info_lifetimes();
    interleaved_histories();
    relocated_orbs();
    fault_isolation();
    card_value_lifetimes();
    host_overflow_aborts();
    card_reference_lifetimes();
    reused_slots_preserve_instance_rules();
}
