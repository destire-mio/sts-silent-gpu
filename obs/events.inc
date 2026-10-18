#pragma once
#include "obs/entities.hpp"

namespace spire::obs {
SPIRE_FN inline int event_page(const Game& g) {
    switch (g.event.id) {
    case EventId::Neow: return g.event.state.Neow.screen;
    case EventId::Falling: return g.event.state.Falling.screen;
    case EventId::MindBloom: return g.event.state.MindBloom.screen;
    case EventId::MoaiHead: return g.event.state.MoaiHead.screenNum;
    case EventId::MysteriousSphere: return g.event.state.MysteriousSphere.screen;
    case EventId::SecretPortal: return g.event.state.SecretPortal.screen;
    case EventId::SensoryStone: return g.event.state.SensoryStone.screen;
    case EventId::TombRedMask: return g.event.state.TombRedMask.screen;
    case EventId::WindingHalls: return g.event.state.WindingHalls.screenNum;
    case EventId::Addict: return g.event.state.Addict.screenNum;
    case EventId::BackToBasics: return g.event.state.BackToBasics.screen;
    case EventId::Beggar: return g.event.state.Beggar.screen;
    case EventId::Colosseum: return g.event.state.Colosseum.screen;
    case EventId::CursedTome: return g.event.state.CursedTome.screen;
    case EventId::DrugDealer: return g.event.state.DrugDealer.screenNum;
    case EventId::ForgottenAltar: return g.event.screen;
    case EventId::Ghosts: return g.event.state.Ghosts.screenNum;
    case EventId::KnowingSkull: return g.event.state.KnowingSkull.screen;
    case EventId::MaskedBandits: return g.event.state.MaskedBandits.screen;
    case EventId::Nest: return g.event.state.Nest.screenNum;
    case EventId::TheJoust: return g.event.state.TheJoust.screen;
    case EventId::TheLibrary: return g.event.state.TheLibrary.screenNum;
    case EventId::TheMausoleum: return g.event.state.TheMausoleum.screen;
    case EventId::Vampires: return g.event.state.Vampires.screenNum;
    case EventId::BigFish: return g.event.state.BigFish.screen;
    case EventId::Cleric: return g.event.screen;
    case EventId::DeadAdventurer: return g.event.state.DeadAdventurer.screen;
    case EventId::GoldenIdolEvent: return g.event.state.GoldenIdolEvent.screenNum;
    case EventId::GoldenWing: return g.event.state.GoldenWing.screen;
    case EventId::GoopPuddle: return g.event.state.GoopPuddle.screen;
    case EventId::LivingWall: return g.event.state.LivingWall.screen;
    case EventId::Mushrooms: return g.event.state.Mushrooms.screenNum;
    case EventId::ScrapOoze: return g.event.state.ScrapOoze.screenNum;
    case EventId::ShiningLight: return g.event.state.ShiningLight.screen;
    case EventId::Sssserpent: return g.event.state.Sssserpent.screen;
    case EventId::AccursedBlacksmith: return g.event.state.AccursedBlacksmith.screenNum;
    case EventId::Bonfire: return g.event.state.Bonfire.screen;
    case EventId::Designer: return g.event.state.Designer.curScreen;
    case EventId::Duplicator: return g.event.state.Duplicator.screenNum;
    case EventId::FaceTrader: return g.event.state.FaceTrader.screen;
    case EventId::FountainOfCurseRemoval: return g.event.state.FountainOfCurseRemoval.screenNum;
    case EventId::GoldShrine: return g.event.state.GoldShrine.screen;
    case EventId::GremlinMatchGame: return g.event.state.GremlinMatchGame.screen;
    case EventId::GremlinWheelGame: return g.event.state.GremlinWheelGame.screen;
    case EventId::Lab: return g.event.state.Lab.screen;
    case EventId::Nloth: return g.event.state.Nloth.screenNum;
    case EventId::PurificationShrine: return g.event.state.PurificationShrine.screen;
    case EventId::Transmogrifier: return g.event.state.Transmogrifier.screen;
    case EventId::UpgradeShrine: return g.event.state.UpgradeShrine.screen;
    case EventId::WeMeetAgain: return g.event.state.WeMeetAgain.screen;
    case EventId::WomanInBlue: return g.event.state.WomanInBlue.screen;
    case EventId::SpireHeart: return g.event.state.SpireHeart.screen;
    case EventId::Count: break;
    }
    SPIRE_ASSERT(false);
    return 0;
}
SPIRE_FN inline void event_values(const Game& g, int slot, bool disabled, Token& t) {
    const int page = event_page(g);
    const auto& s = g.event.state;
    switch (g.event.id) {
    case EventId::Neow:
        if (page == 3) {
            t.values[1] = s.Neow.choices[slot] + 1;
            t.values[2] = s.Neow.drawbacks[slot] + 1;
            if (s.Neow.choices[slot] == units::events::Neow::HpBonus || s.Neow.choices[slot] == units::events::Neow::LargeHpBonus ||
                s.Neow.drawbacks[slot] == units::events::Neow::HpLoss) t.values[3] = s.Neow.hp_bonus;
        }
        break;
    case EventId::BigFish: if (page == 0 && slot == 0) t.values[1] = s.BigFish.healAmt; break;
    case EventId::Cleric:
        if (page == 0 && !disabled) {
            if (slot == 0) { t.values[1] = s.Cleric.healAmt; t.values[2] = 35; }
            if (slot == 1) t.values[2] = s.Cleric.purifyCost;
        }
        break;
    case EventId::ForgottenAltar: if (page == 0 && slot == 1) t.values[1] = s.ForgottenAltar.hpLoss; break;
    case EventId::GoldenIdolEvent:
        if (page == 1 && slot == 1) t.values[1] = s.GoldenIdolEvent.damage;
        if (page == 1 && slot == 2) t.values[1] = s.GoldenIdolEvent.maxHpLoss;
        break;
    case EventId::Ghosts: if (page == 0 && slot == 0) t.values[1] = s.Ghosts.hpLoss; break;
    case EventId::Vampires: if (page == 0 && slot == 0) t.values[1] = s.Vampires.maxHpLoss; break;
    case EventId::MoaiHead: if (page == 0 && slot == 0) t.values[1] = s.MoaiHead.hpAmt; break;
    case EventId::ShiningLight: if (page == 0 && slot == 0 && !disabled) t.values[1] = s.ShiningLight.damage; break;
    case EventId::TheLibrary: if (page == 0 && slot == 1) t.values[1] = s.TheLibrary.healAmt; break;
    case EventId::WindingHalls:
        if (page == 1) t.values[1] = slot == 0 ? s.WindingHalls.hpAmt : slot == 1 ? s.WindingHalls.healAmt : s.WindingHalls.maxHPAmt;
        break;
    case EventId::CursedTome: if (page == 4 && slot == 0) t.values[1] = s.CursedTome.finalDmg; break;
    case EventId::FaceTrader:
        if (page == 1 && slot == 0) { t.values[1] = g.unit_statics.FaceTrader.damage; t.values[2] = g.unit_statics.FaceTrader.goldReward; }
        break;
    case EventId::GoopPuddle:
        if (page == 0) {
            t.values[1] = slot == 0 ? 11 : s.GoopPuddle.goldLoss;
            if (slot == 0) t.values[2] = 75;
        }
        break;
    case EventId::ScrapOoze:
        if (page == 0 && slot == 0) { t.values[1] = s.ScrapOoze.dmg; t.values[2] = s.ScrapOoze.relicObtainChance; }
        break;
    case EventId::DeadAdventurer:
        if (page == 0) {
            t.values[1] = s.DeadAdventurer.enemy + 1;
            if (slot == 0) t.values[2] = s.DeadAdventurer.encounterChance;
        }
        break;
    case EventId::KnowingSkull:
        if (page == 1) {
            t.values[1] = slot == 0 ? s.KnowingSkull.potionCost : slot == 1 ? s.KnowingSkull.goldCost : slot == 2 ? s.KnowingSkull.cardCost : s.KnowingSkull.leaveCost;
            if (slot == 1) t.values[2] = 90;
        }
        break;
    case EventId::Designer:
        if (page == 1) {
            if (slot == 0) { t.values[1] = s.Designer.adjustmentUpgradesOne + 1; t.values[2] = 50; }
            if (slot == 1) { t.values[1] = s.Designer.cleanUpRemovesCards + 1; t.values[2] = 75; }
            if (slot == 2) t.values[2] = 110;
            if (slot == 3) t.values[1] = 5;
        }
        break;
    case EventId::WeMeetAgain: if (page == 0 && slot == 1 && !disabled) t.values[2] = s.WeMeetAgain.goldAmount; break;
    case EventId::GremlinWheelGame:
        if (page == 1) {
            t.values[1] = s.GremlinWheelGame.result + 1;
            if (s.GremlinWheelGame.result == 0) t.values[2] = s.GremlinWheelGame.goldAmount;
            if (s.GremlinWheelGame.result == 5) t.values[2] = static_cast<int>(g.player.creature.max_hp * 0.15f);
        }
        break;
    default: break;
    }
}
SPIRE_FN inline void event_entities(Writer& w, const Game& g, int* choices) {
    const int page = event_page(g);
    const auto& s = g.event.state;
    if (g.event.id == EventId::GremlinMatchGame && page == 2) {
        int choice = 0;
        for (int i = 0; i < s.GremlinMatchGame.count; ++i) {
            const Card& card = g.cards[s.GremlinMatchGame.cards[i]];
            Token t = card.is_flipped ? option(Control::CardBack, i + 1) : card_token(g, card, Zone::Event, i + 1, false);
            t.values[10] = s.GremlinMatchGame.attemptCount;
            const int entity = w.add(t);
            if (card.is_flipped) choices[choice++] = entity;
        }
        return;
    }
    if (g.event.id == EventId::GremlinWheelGame && s.GremlinWheelGame.startSpin) {
        choices[0] = w.add(option(Control::Spin, 1));
        return;
    }
    const auto& dialog = event_data(g.event.id).image ? g.image_dialog : g.room_dialog;
    int choice = 0;
    for (int i = 0; i < dialog.size; ++i) {
        const int slot = dialog[i].slot;
        Token t = option(Control::EventButton, slot + 1, static_cast<int>(g.event.id) + 1);
        t.values[0] = dialog[i].disabled;
        t.values[9] = page;
        event_values(g, slot, dialog[i].disabled, t);
        const int entity = w.add(t);
        if (!dialog[i].disabled) choices[choice++] = entity;
        if (g.event.id == EventId::Falling && page == 1 && !dialog[i].disabled) {
            const uint16_t uuid = slot == 0 ? s.Falling.skillCard : slot == 1 ? s.Falling.powerCard : s.Falling.attackCard;
            master_offer(w, g, uuid, entity);
        }
        if (g.event.id == EventId::WeMeetAgain && page == 0 && !dialog[i].disabled) {
            if (slot == 0) {
                const auto& reward = g.reward_objects[s.WeMeetAgain.potionOption - 1];
                w.add(token(Kind::Potion, static_cast<int>(reward.potion) + 1, Zone::Event, 0, entity));
            }
            if (slot == 2) master_offer(w, g, s.WeMeetAgain.cardOption, entity);
        }
        if (g.event.id == EventId::Nloth && page == 0 && slot < 2) {
            Token offer = relic_token(g.reward_objects[slot == 0 ? s.Nloth.choice1 : s.Nloth.choice2].relic, Zone::Event, 0, false);
            offer.ids[3] = entity; w.add(offer);
        }
    }
}
}
