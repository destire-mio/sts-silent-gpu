package oracle;

import com.google.gson.JsonObject;
import com.megacrit.cardcrawl.cards.AbstractCard;
import com.megacrit.cardcrawl.cards.CardQueueItem;
import com.megacrit.cardcrawl.dungeons.AbstractDungeon;
import com.megacrit.cardcrawl.monsters.AbstractMonster;
import com.megacrit.cardcrawl.potions.AbstractPotion;
import com.megacrit.cardcrawl.potions.PotionSlot;
import com.megacrit.cardcrawl.relics.AbstractRelic;
import com.megacrit.cardcrawl.rooms.AbstractRoom;
import communicationmod.ChoiceScreenUtils;
import communicationmod.GameStateListener;
import java.util.ArrayList;

final class Actions {
    static JsonObject action(String type, int... args) {
        JsonObject result = new JsonObject();
        result.addProperty("type", type);
        if (type.equals("play")) { result.addProperty("card", args[0]); result.addProperty("target", args[1]); }
        if (type.equals("choose")) result.addProperty("option", args[0]);
        if (type.startsWith("potion_")) result.addProperty("slot", args[0]);
        if (type.equals("potion_use")) result.addProperty("target", args[1]);
        return result;
    }

    static ArrayList<JsonObject> legal() {
        ArrayList<JsonObject> result = new ArrayList<JsonObject>();
        if (AbstractDungeon.getCurrRoom().phase == AbstractRoom.RoomPhase.COMBAT && !AbstractDungeon.isScreenUp) {
            for (int i = 0; i < AbstractDungeon.player.hand.size(); i++) {
                AbstractCard card = AbstractDungeon.player.hand.group.get(i);
                if (card.target == AbstractCard.CardTarget.ENEMY || card.target == AbstractCard.CardTarget.SELF_AND_ENEMY) {
                    for (int j = 0; j < AbstractDungeon.getMonsters().monsters.size(); j++) {
                        AbstractMonster monster = AbstractDungeon.getMonsters().monsters.get(j);
                        if (!monster.isDeadOrEscaped() && !monster.halfDead && card.canUse(AbstractDungeon.player, monster))
                            result.add(action("play", i, j));
                    }
                } else if (card.canUse(AbstractDungeon.player, null)) result.add(action("play", i, -1));
            }
            result.add(action("end_turn"));
        }
        for (AbstractPotion potion : AbstractDungeon.player.potions) {
            if (potion instanceof PotionSlot) continue;
            if (potion.canUse()) {
                if (potion.targetRequired) {
                    for (int j = 0; j < AbstractDungeon.getMonsters().monsters.size(); j++) {
                        AbstractMonster monster = AbstractDungeon.getMonsters().monsters.get(j);
                        if (!monster.isDeadOrEscaped() && !monster.halfDead) result.add(action("potion_use", potion.slot, j));
                    }
                } else result.add(action("potion_use", potion.slot, -1));
            }
            if (potion.canDiscard()) result.add(action("potion_discard", potion.slot));
        }
        for (int i = 0; i < ChoiceScreenUtils.getCurrentChoiceList().size(); i++) result.add(action("choose", i));
        if (ChoiceScreenUtils.isConfirmButtonAvailable()) result.add(action("proceed"));
        if (ChoiceScreenUtils.isCancelButtonAvailable()) result.add(action("skip"));
        return result;
    }

    static void apply(JsonObject action) {
        boolean grid = AbstractDungeon.screen == AbstractDungeon.CurrentScreen.GRID
            && AbstractDungeon.getCurrRoom().phase != AbstractRoom.RoomPhase.COMBAT;
        switch (action.get("type").getAsString()) {
            case "play": {
                int target = action.get("target").getAsInt();
                AbstractDungeon.actionManager.cardQueue.add(new CardQueueItem(
                    AbstractDungeon.player.hand.group.get(action.get("card").getAsInt()),
                    target < 0 ? null : AbstractDungeon.getMonsters().monsters.get(target)));
                break;
            }
            case "end_turn": AbstractDungeon.overlayMenu.endTurnButton.disable(true); break;
            case "choose": ChoiceScreenUtils.executeChoice(action.get("option").getAsInt()); break;
            case "proceed": ChoiceScreenUtils.pressConfirmButton(); break;
            case "skip": ChoiceScreenUtils.pressCancelButton(); break;
            case "potion_use": {
                AbstractPotion potion = AbstractDungeon.player.potions.get(action.get("slot").getAsInt());
                int target = action.get("target").getAsInt();
                potion.use(target < 0 ? AbstractDungeon.player : AbstractDungeon.getMonsters().monsters.get(target));
                for (AbstractRelic relic : AbstractDungeon.player.relics) relic.onUsePotion();
                AbstractDungeon.topPanel.destroyPotion(potion.slot);
                break;
            }
            case "potion_discard": AbstractDungeon.topPanel.destroyPotion(action.get("slot").getAsInt()); break;
            default: throw new AssertionError(action);
        }
        GameStateListener.registerCommandExecution();
        GameStateListener.registerStateChange();
        if (grid) GameStateListener.setTimeout(1);
    }
}
