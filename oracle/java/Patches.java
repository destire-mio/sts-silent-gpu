package oracle;

import com.evacipated.cardcrawl.modthespire.lib.*;
import com.megacrit.cardcrawl.desktop.DesktopLauncher;
import com.megacrit.cardcrawl.core.Settings;
import com.megacrit.cardcrawl.core.CardCrawlGame;
import com.megacrit.cardcrawl.unlock.UnlockTracker;
import com.megacrit.cardcrawl.integrations.DistributorFactory;
import com.megacrit.cardcrawl.integrations.PublisherIntegration;
import com.megacrit.cardcrawl.integrations.gog.GogIntegration;
import javassist.*;
import javassist.expr.*;

public final class Patches {
    @SpirePatch(clz=DistributorFactory.class, method="getEnabledDistributor")
    public static class OfflinePublisher {
        @SpirePrefixPatch public static SpireReturn<PublisherIntegration> before() {
            return SpireReturn.Return(new GogIntegration());
        }
    }

    @SpirePatch(clz=CardCrawlGame.class, method="create")
    public static class NoSteamOverlay {
        @SpireInstrumentPatch public static ExprEditor instrument() {
            return new ExprEditor() {
                public void edit(NewExpr expression) throws CannotCompileException {
                    if (expression.getClassName().equals("com.codedisaster.steamworks.SteamUtils"))
                        expression.replace("{ $_ = null; }");
                }
            };
        }
    }

    @SpirePatch(clz=UnlockTracker.class, method="isCardLocked")
    public static class Cards {
        @SpirePrefixPatch public static SpireReturn<Boolean> before() { return SpireReturn.Return(false); }
    }
    @SpirePatch(clz=UnlockTracker.class, method="isRelicLocked")
    public static class Relics {
        @SpirePrefixPatch public static SpireReturn<Boolean> before() { return SpireReturn.Return(false); }
    }
    @SpirePatch(clz=UnlockTracker.class, method="isAscensionUnlocked")
    public static class Ascension {
        @SpirePrefixPatch public static SpireReturn<Boolean> before() { return SpireReturn.Return(true); }
    }
    @SpirePatch(clz=Settings.class, method="setFinalActAvailability")
    public static class ActFour {
        @SpirePostfixPatch public static void after() { Settings.isFinalActAvailable = true; }
    }

    @SpirePatch(clz=DesktopLauncher.class, method="main")
    public static class Randomness {
        @SpireRawPatch public static void patch(CtBehavior method) throws Exception {
            ClassPool pool = method.getDeclaringClass().getClassPool();
            for (String name : new String[]{"cards.CardGroup", "monsters.MonsterGroup", "random.Random",
                    "characters.AbstractPlayer", "dungeons.AbstractDungeon", "relics.Test5",
                    "actions.common.DiscardAtEndOfTurnAction", "helpers.MonsterHelper", "cards.AbstractCard"}) {
                CtClass cls = pool.get("com.megacrit.cardcrawl." + name);
                cls.instrument(new ExprEditor() {
                    public void edit(MethodCall call) throws CannotCompileException {
                        String owner = call.getClassName(), name = call.getMethodName();
                        if (owner.equals("java.util.Collections") && name.equals("shuffle")
                                && call.getSignature().equals("(Ljava/util/List;)V"))
                            call.replace("{ java.util.Collections.shuffle($1, new java.util.Random(oracle.Unseeded.rng.nextLong())); }");
                        if (owner.equals("java.util.UUID") && name.equals("randomUUID"))
                            call.replace("{ $_ = new java.util.UUID(0L, ++oracle.Unseeded.uuid); }");
                        if (owner.equals("com.badlogic.gdx.math.MathUtils") && name.startsWith("random")
                                && (!call.where().getDeclaringClass().getName().endsWith("MonsterHelper") || call.where().getName().equals("randomXOffset")))
                            call.replace("{ $_ = oracle.Unseeded." + name + "($$); }");
                    }
                });
            }
            pool.get("com.megacrit.cardcrawl.helpers.GameTips").instrument(new ExprEditor() {
                public void edit(MethodCall call) throws CannotCompileException {
                    if (call.getClassName().equals("java.util.Collections") && call.getMethodName().equals("shuffle"))
                        call.replace("{ java.util.Collections.shuffle($1, new java.util.Random(com.badlogic.gdx.math.MathUtils.random.nextLong())); }");
                }
            });
            pool.get("com.megacrit.cardcrawl.screens.select.GridCardSelectScreen").getDeclaredMethod("update").instrument(new ExprEditor() {
                public void edit(MethodCall call) throws CannotCompileException {
                    if (call.getClassName().equals("com.megacrit.cardcrawl.dungeons.AbstractDungeon")
                            && call.getMethodName().equals("returnTrulyRandomCardFromAvailable"))
                        call.replace("{ $_ = com.megacrit.cardcrawl.dungeons.AbstractDungeon.returnTrulyRandomCardFromAvailable($1, new com.megacrit.cardcrawl.random.Random(java.lang.Long.valueOf(com.badlogic.gdx.math.MathUtils.random(9999L)), com.badlogic.gdx.math.MathUtils.random(99))); }");
                }
            });
            for (String name : new String[]{"ObjectSet", "ObjectMap", "IntMap", "LongMap", "IntIntMap", "IntFloatMap", "ObjectIntMap", "ObjectFloatMap"}) {
                pool.get("com.badlogic.gdx.utils." + name).instrument(new ExprEditor() {
                    public void edit(MethodCall call) throws CannotCompileException {
                        if (call.getClassName().equals("com.badlogic.gdx.math.MathUtils") && call.getMethodName().equals("random"))
                            call.replace("{ $_ = oracle.Platform.deviceRandom.nextInt($1 + 1); }");
                    }
                });
            }
            CtClass texture = pool.get("com.badlogic.gdx.graphics.Texture");
            texture.addField(CtField.make("private int oracleId;", texture));
            texture.addMethod(CtMethod.make("public int hashCode() { if (oracleId == 0) oracleId = ++oracle.Platform.nextTextureIdentity; return oracleId; }", texture));
        }
    }
}
