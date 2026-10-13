package oracle;

import com.megacrit.cardcrawl.cards.*;
import com.megacrit.cardcrawl.dungeons.AbstractDungeon;
import com.megacrit.cardcrawl.events.*;
import com.megacrit.cardcrawl.events.shrines.*;
import com.megacrit.cardcrawl.helpers.Hitbox;
import com.megacrit.cardcrawl.helpers.input.InputHelper;
import com.megacrit.cardcrawl.map.MapRoomNode;
import com.megacrit.cardcrawl.relics.AbstractRelic;
import com.megacrit.cardcrawl.rewards.RewardItem;
import com.megacrit.cardcrawl.rewards.chests.AbstractChest;
import com.megacrit.cardcrawl.rooms.*;
import com.megacrit.cardcrawl.shop.*;
import com.megacrit.cardcrawl.ui.buttons.LargeDialogOptionButton;
import com.megacrit.cardcrawl.ui.campfire.AbstractCampfireOption;
import java.lang.reflect.*;
import java.util.*;

final class Choices {
    static Field field(Object object, String name) throws Exception {
        Class<?> cls = object instanceof Class<?> ? (Class<?>)object : object.getClass();
        while (!Arrays.asList(Arrays.stream(cls.getDeclaredFields()).map(Field::getName).toArray(String[]::new)).contains(name)) cls = cls.getSuperclass();
        Field field = cls.getDeclaredField(name);
        field.setAccessible(true);
        return field;
    }
    @SuppressWarnings("unchecked")
    static <T> T get(Object object, String name) throws Exception {
        return (T)field(object, name).get(object instanceof Class<?> ? null : object);
    }
    static void set(Object object, String name, Object value) throws Exception {
        field(object, name).set(object instanceof Class<?> ? null : object, value);
    }
    static void call(Object object, Class<?> owner, String name) throws Exception {
        Method method = owner.getDeclaredMethod(name);
        method.setAccessible(true);
        method.invoke(object);
    }
    static void point(Hitbox hb) {
        InputHelper.mX = (int)hb.cX;
        InputHelper.mY = (int)hb.cY;
    }
    static void click(Hitbox hb) {
        point(hb);
        hb.clicked = true;
    }
    static String type() {
        if (AbstractDungeon.isScreenUp) return AbstractDungeon.screen.name();
        AbstractRoom room = AbstractDungeon.getCurrRoom();
        if (room.phase == AbstractRoom.RoomPhase.EVENT || room.event != null && room.phase == AbstractRoom.RoomPhase.COMPLETE) return "EVENT";
        if (room instanceof TreasureRoom || room instanceof TreasureRoomBoss) return "CHEST";
        if (room instanceof ShopRoom) return "SHOP_ROOM";
        if (room instanceof RestRoom) return "REST";
        return room.phase == AbstractRoom.RoomPhase.COMPLETE ? "COMPLETE" : "NONE";
    }
    static boolean boss() {
        return AbstractDungeon.currMapNode.y == 14 || AbstractDungeon.id.equals("TheEnding") && AbstractDungeon.currMapNode.y == 2;
    }
    static AbstractChest chest() {
        AbstractRoom room = AbstractDungeon.getCurrRoom();
        return room instanceof TreasureRoom ? ((TreasureRoom)room).chest : ((TreasureRoomBoss)room).chest;
    }
    static boolean imageDialog() throws Exception { return get(GenericEventDialog.class, "show"); }
    static boolean visible(Object button) throws Exception { return !Choices.<Boolean>get(button, "isHidden"); }

    static ArrayList<Object> options() throws Exception {
        ArrayList<Object> out = new ArrayList<Object>();
        AbstractRoom room = AbstractDungeon.getCurrRoom();
        switch (type()) {
            case "EVENT": {
                List<LargeDialogOptionButton> buttons = imageDialog() ? room.event.imageEventText.optionList
                    : Choices.<Boolean>get(room.event.roomEventText, "show") ? RoomEventDialog.optionList : Collections.<LargeDialogOptionButton>emptyList();
                for (LargeDialogOptionButton button : buttons) if (!button.isDisabled) out.add(button);
                if (!buttons.isEmpty()) break;
                if (room.event instanceof GremlinWheelGame && Choices.<Boolean>get(room.event, "startSpin") && !Choices.<Boolean>get(room.event, "buttonPressed")) out.add("spin");
                if (room.event instanceof GremlinMatchGame && Choices.<Float>get(room.event, "waitTimer") == 0.0f)
                    for (AbstractCard card : Choices.<CardGroup>get(room.event, "cards").group) if (card.isFlipped) out.add(card);
                break;
            }
            case "CHEST": if (!chest().isOpen) out.add("open"); break;
            case "SHOP_ROOM": out.add("shop"); break;
            case "REST":
                if (!((RestRoom)room).campfireUI.somethingSelected)
                    for (AbstractCampfireOption option : Choices.<ArrayList<AbstractCampfireOption>>get(((RestRoom)room).campfireUI, "buttons")) if (option.usable) out.add(option);
                break;
            case "MAP":
                if (boss()) out.add("boss");
                else for (ArrayList<MapRoomNode> row : AbstractDungeon.map) for (MapRoomNode node : row)
                    if (node.hasEdges() && (!AbstractDungeon.firstRoomChosen ? node.y == 0
                        : AbstractDungeon.currMapNode.isConnectedTo(node) || AbstractDungeon.currMapNode.wingedIsConnectedTo(node))) out.add(node);
                break;
            case "GRID":
                if (!AbstractDungeon.gridSelectScreen.confirmScreenUp && !AbstractDungeon.gridSelectScreen.isJustForConfirming)
                    out.addAll(Choices.<CardGroup>get(AbstractDungeon.gridSelectScreen, "targetGroup").group);
                break;
            case "HAND_SELECT":
                if (AbstractDungeon.handCardSelectScreen.numCardsToSelect > AbstractDungeon.handCardSelectScreen.selectedCards.size()) out.addAll(AbstractDungeon.player.hand.group);
                break;
            case "CARD_REWARD":
                out.addAll(AbstractDungeon.cardRewardScreen.rewardGroup);
                if (visible(get(AbstractDungeon.cardRewardScreen, "bowlButton"))) out.add("bowl");
                break;
            case "COMBAT_REWARD": out.addAll(AbstractDungeon.combatRewardScreen.rewards); break;
            case "BOSS_REWARD": out.addAll(AbstractDungeon.bossRelicScreen.relics); break;
            case "SHOP":
                if (AbstractDungeon.shopScreen.purgeAvailable && AbstractDungeon.player.gold >= ShopScreen.actualPurgeCost) out.add("purge");
                for (String name : new String[]{"coloredCards", "colorlessCards"})
                    for (AbstractCard card : Choices.<ArrayList<AbstractCard>>get(AbstractDungeon.shopScreen, name)) if (card.price <= AbstractDungeon.player.gold) out.add(card);
                for (StoreRelic relic : Choices.<ArrayList<StoreRelic>>get(AbstractDungeon.shopScreen, "relics")) if (relic.price <= AbstractDungeon.player.gold) out.add(relic);
                for (StorePotion potion : Choices.<ArrayList<StorePotion>>get(AbstractDungeon.shopScreen, "potions")) if (potion.price <= AbstractDungeon.player.gold) out.add(potion);
                break;
        }
        return out;
    }
    static ArrayList<String> names() throws Exception {
        ArrayList<String> names = new ArrayList<String>();
        for (Object option : options()) {
            String name;
            if (option instanceof AbstractCard) name = ((AbstractCard)option).cardID;
            else if (option instanceof AbstractRelic) name = ((AbstractRelic)option).relicId;
            else if (option instanceof StoreRelic) name = ((StoreRelic)option).relic.relicId;
            else if (option instanceof StorePotion) name = ((StorePotion)option).potion.ID;
            else if (option instanceof RewardItem) name = ((RewardItem)option).type.name();
            else if (option instanceof LargeDialogOptionButton) name = "option:" + ((LargeDialogOptionButton)option).slot;
            else if (option instanceof MapRoomNode) name = "x=" + ((MapRoomNode)option).x;
            else if (option instanceof String) name = (String)option;
            else name = option.getClass().getSimpleName();
            names.add(name);
        }
        return names;
    }
    static boolean confirm() throws Exception {
        switch (type()) {
            case "GRID": return visible(AbstractDungeon.gridSelectScreen.confirmButton) && !AbstractDungeon.gridSelectScreen.confirmButton.isDisabled;
            case "HAND_SELECT": return visible(AbstractDungeon.handCardSelectScreen.button) && !AbstractDungeon.handCardSelectScreen.button.isDisabled;
            case "CHEST": case "SHOP_ROOM": case "REST": case "COMPLETE": case "COMBAT_REWARD":
                return !Choices.<Boolean>get(AbstractDungeon.overlayMenu.proceedButton, "isHidden");
            default: return false;
        }
    }
    static boolean cancel() throws Exception {
        switch (type()) {
            case "CARD_REWARD": return visible(get(AbstractDungeon.cardRewardScreen, "skipButton"));
            case "BOSS_REWARD": return visible(get(AbstractDungeon.bossRelicScreen, "cancelButton"));
            case "GRID": case "MAP": case "SHOP": case "COMBAT_REWARD": return visible(AbstractDungeon.overlayMenu.cancelButton);
            default: return false;
        }
    }
    static void choose(int index) throws Exception {
        Object choice = options().get(index);
        switch (type()) {
            case "EVENT":
                if (choice instanceof LargeDialogOptionButton) ((LargeDialogOptionButton)choice).pressed = true;
                else if (choice instanceof AbstractCard) {
                    point(((AbstractCard)choice).hb); InputHelper.justClickedLeft = true;
                    call(AbstractDungeon.getCurrRoom().event, GremlinMatchGame.class, "updateMatchGameLogic");
                } else set(AbstractDungeon.getCurrRoom().event, "buttonPressed", true);
                break;
            case "CHEST": chest().isOpen = true; chest().open(false); break;
            case "SHOP_ROOM": AbstractDungeon.shopScreen.open(); break;
            case "REST":
                ((AbstractCampfireOption)choice).useOption();
                ((RestRoom)AbstractDungeon.getCurrRoom()).campfireUI.somethingSelected = true;
                break;
            case "MAP":
                if (choice instanceof MapRoomNode) {
                    MapRoomNode node = (MapRoomNode)choice;
                    node.update(); point(node.hb); AbstractDungeon.dungeonMapScreen.clicked = true; node.update();
                    if (Choices.<Float>get(node, "animWaitTimer") == 0.0f) throw new AssertionError("Map input was not accepted");
                } else {
                    point(AbstractDungeon.dungeonMapScreen.map.bossHb); InputHelper.justClickedLeft = true;
                    AbstractDungeon.dungeonMapScreen.map.update();
                }
                break;
            case "GRID":
                AbstractCard selected = (AbstractCard)choice;
                boolean wasSelected = AbstractDungeon.gridSelectScreen.selectedCards.contains(selected);
                boolean moving;
                do {
                    call(AbstractDungeon.gridSelectScreen, AbstractDungeon.gridSelectScreen.getClass(), "updateCardPositionsAndHoverLogic");
                    moving = false;
                    for (AbstractCard card : Choices.<CardGroup>get(AbstractDungeon.gridSelectScreen, "targetGroup").group)
                        moving |= card.current_x != card.target_x || card.current_y != card.target_y;
                } while (moving);
                click(selected.hb); AbstractDungeon.gridSelectScreen.update();
                if (AbstractDungeon.isScreenUp && AbstractDungeon.screen == AbstractDungeon.CurrentScreen.GRID
                        && !AbstractDungeon.gridSelectScreen.confirmScreenUp && AbstractDungeon.gridSelectScreen.selectedCards.contains(selected) == wasSelected)
                    throw new AssertionError("Grid input was not accepted");
                break;
            case "HAND_SELECT":
                AbstractDungeon.handCardSelectScreen.hoveredCard = (AbstractCard)choice;
                call(AbstractDungeon.handCardSelectScreen, AbstractDungeon.handCardSelectScreen.getClass(), "selectHoveredCard"); break;
            case "CARD_REWARD":
                if (choice instanceof AbstractCard) {
                    click(((AbstractCard)choice).hb); AbstractDungeon.cardRewardScreen.update();
                } else {
                    com.megacrit.cardcrawl.ui.buttons.SingingBowlButton button = get(AbstractDungeon.cardRewardScreen, "bowlButton");
                    button.onClick(); AbstractDungeon.cardRewardScreen.closeFromBowlButton(); AbstractDungeon.closeCurrentScreen();
                }
                break;
            case "COMBAT_REWARD": ((RewardItem)choice).isDone = true; break;
            case "BOSS_REWARD":
                point(((AbstractRelic)choice).hb); InputHelper.justClickedLeft = true; AbstractDungeon.bossRelicScreen.update(); break;
            case "SHOP":
                if (choice instanceof AbstractCard) {
                    Method method = ShopScreen.class.getDeclaredMethod("purchaseCard", AbstractCard.class);
                    method.setAccessible(true); method.invoke(AbstractDungeon.shopScreen, choice);
                } else if (choice instanceof StoreRelic) ((StoreRelic)choice).purchaseRelic();
                else if (choice instanceof StorePotion) ((StorePotion)choice).purchasePotion();
                else call(AbstractDungeon.shopScreen, ShopScreen.class, "purchasePurge");
                break;
            default: throw new AssertionError(type());
        }
        InputHelper.updateLast();
    }
    static void proceed() throws Exception {
        switch (type()) {
            case "GRID": AbstractDungeon.gridSelectScreen.confirmButton.hb.clicked = true; break;
            case "HAND_SELECT": AbstractDungeon.handCardSelectScreen.button.hb.clicked = true; break;
            default: Choices.<Hitbox>get(AbstractDungeon.overlayMenu.proceedButton, "hb").clicked = true;
        }
    }
    static void skip() throws Exception {
        switch (type()) {
            case "CARD_REWARD":
                com.megacrit.cardcrawl.ui.buttons.SkipCardButton button = get(AbstractDungeon.cardRewardScreen, "skipButton");
                button.hb.clicked = true; break;
            case "BOSS_REWARD":
                com.megacrit.cardcrawl.screens.mainMenu.MenuCancelButton cancel = get(AbstractDungeon.bossRelicScreen, "cancelButton");
                cancel.hb.clicked = true; break;
            default: AbstractDungeon.overlayMenu.cancelButton.hb.clicked = true;
        }
    }
}
