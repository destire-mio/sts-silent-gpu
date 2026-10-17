package oracle.test;

import java.io.PrintWriter;
import java.lang.instrument.*;
import java.security.ProtectionDomain;
import javassist.*;

public final class OracleSoulFrames {
    public static void premain(String argument, Instrumentation instrumentation) throws Exception {
        ClassPool pool = new ClassPool(true);
        pool.insertClassPath("mods/Oracle.jar");
        pool.insertClassPath("desktop-1.0.jar");
        CtClass cls = pool.get("oracle.Recorder");
        cls.addField(CtField.make("static java.util.IdentityHashMap delayedSouls = new java.util.IdentityHashMap();", cls));
        cls.addMethod(CtNewMethod.make(
            "static void delaySouls() throws Exception {" +
            " if (com.megacrit.cardcrawl.dungeons.AbstractDungeon.player == null ||" +
            "     com.megacrit.cardcrawl.dungeons.AbstractDungeon.getCurrMapNode() == null) return;" +
            " Object group = com.megacrit.cardcrawl.dungeons.AbstractDungeon.getCurrRoom().souls;" +
            " java.lang.reflect.Field field = com.megacrit.cardcrawl.cards.SoulGroup.class.getDeclaredField(\"souls\");" +
            " field.setAccessible(true);" +
            " java.util.ArrayList souls = (java.util.ArrayList)field.get(group);" +
            " java.lang.reflect.Field timer = com.megacrit.cardcrawl.cards.Soul.class.getDeclaredField(\"spawnStutterTimer\");" +
            " timer.setAccessible(true);" +
            " for (int i = 0; i < souls.size(); i++) {" +
            "   com.megacrit.cardcrawl.cards.Soul soul = (com.megacrit.cardcrawl.cards.Soul)souls.get(i);" +
            "   if (soul.isReadyForReuse) delayedSouls.remove(soul);" +
            "   else if (!delayedSouls.containsKey(soul)) {" +
            "     timer.setFloat(soul, timer.getFloat(soul) + 2.0f);" +
            "     delayedSouls.put(soul, java.lang.Boolean.TRUE);" +
            "   }" +
            " }" +
            "}", cls));
        cls.getDeclaredMethod("tick").insertAfter("delaySouls();");
        final byte[] delayed = cls.toBytecode();
        final PrintWriter marker = new PrintWriter(argument, "UTF-8");
        instrumentation.addTransformer(new ClassFileTransformer() {
            public byte[] transform(ClassLoader loader, String name, Class<?> redefining,
                                    ProtectionDomain domain, byte[] bytes) {
                if (!name.equals("oracle/Recorder")) return null;
                marker.println("active");
                marker.close();
                return delayed;
            }
        });
    }
}
