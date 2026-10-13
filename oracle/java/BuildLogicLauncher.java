import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.jar.JarEntry;
import java.util.jar.JarOutputStream;
import javassist.ClassPool;
import javassist.CtClass;
import javassist.expr.ExprEditor;
import javassist.expr.MethodCall;

public final class BuildLogicLauncher {
    public static void main(String[] args) throws Exception {
        ClassPool pool = new ClassPool(true);
        pool.insertClassPath(args[0]);
        CtClass loader = pool.get("com.evacipated.cardcrawl.modthespire.Loader");
        final int[] replaced = {0};
        loader.getDeclaredMethod("main").instrument(new ExprEditor() {
            public void edit(MethodCall call) throws javassist.CannotCompileException {
                if (call.getClassName().equals("java.awt.EventQueue") && call.getMethodName().equals("invokeLater")) {
                    call.replace("{ ALLMODINFOS = getAllMods(getWorkshopInfos()); "
                        + "java.io.File[] files = new java.io.File[ALLMODINFOS.length]; "
                        + "for (int i=0;i<files.length;i++) files[i]=new java.io.File(ALLMODINFOS[i].jarURL.toURI()); "
                        + "runMods(files); }");
                    replaced[0]++;
                }
            }
        });
        if (replaced[0] != 1) throw new IllegalStateException("Unexpected MTS launch structure");
        loader.getDeclaredMethod("closeWindow").setBody("{}");
        loader.getDeclaredMethod("restoreWindowOnCrash").setBody("{ System.exit(1); }");
        loader.getDeclaredMethod("runMods").instrument(new ExprEditor() {
            public void edit(MethodCall call) throws javassist.CannotCompileException {
                String name = call.getMethodName();
                if (name.equals("schedule") && call.getClassName().equals("java.util.Timer"))
                    call.replace("{ $0.cancel(); }");
                if (name.equals("showMessageDialog") && call.getClassName().equals("javax.swing.JOptionPane"))
                    call.replace("{ System.err.println($2); }");
            }
        });
        loader.getDeclaredMethod("runMods").insertAfter("{ System.exit(1); }", true);
        try (JarOutputStream out = new JarOutputStream(Files.newOutputStream(Paths.get(args[1])))) {
            JarEntry entry = new JarEntry("com/evacipated/cardcrawl/modthespire/Loader.class");
            entry.setTime(0L);
            out.putNextEntry(entry);
            out.write(loader.toBytecode());
            out.closeEntry();
        }
    }
}
