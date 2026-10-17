import javassist.ClassPool;
import javassist.CtClass;

public final class BuildWatch {
    public static void main(String[] args) throws Exception {
        ClassPool pool = new ClassPool(true);
        for (int i = 1; i < args.length; i++) pool.insertClassPath(args[i]);
        CtClass launcher = pool.get("oracle.Platform$Launcher");
        launcher.getDeclaredMethod("before").setBody("{ oracle.Watch.run(); return com.evacipated.cardcrawl.modthespire.lib.SpireReturn.Return(null); }");
        launcher.writeFile(args[0]);
        CtClass platform = pool.get("oracle.Platform");
        platform.getDeclaredMethod("initializePlatform").insertAfter("{ oracle.Watch.openDisplay(); }");
        platform.writeFile(args[0]);
        CtClass recorder = pool.get("oracle.Recorder");
        recorder.getDeclaredMethod("tick").insertAfter("{ oracle.Watch.present(); }");
        recorder.writeFile(args[0]);
    }
}
