package oracle.test;

import java.io.PrintWriter;
import java.lang.instrument.*;
import java.security.ProtectionDomain;
import javassist.*;
import javassist.expr.*;

public final class OracleIdleFrames {
    public static void premain(String argument, Instrumentation instrumentation) throws Exception {
        ClassPool pool = new ClassPool(true);
        pool.insertClassPath("mods/Oracle.jar");
        pool.insertClassPath("desktop-1.0.jar");
        CtClass cls = pool.get("oracle.Recorder");
        final int[] sites = {0};
        cls.getDeclaredMethod("record").instrument(new ExprEditor() {
            public void edit(MethodCall call) throws CannotCompileException {
                if (call.getClassName().equals("oracle.Snapshot") && call.getMethodName().equals("capture")) {
                    sites[0]++;
                    call.replace("{ for (int i=0; i<13; i++) tick(); $_ = $proceed($$); }");
                }
            }
        });
        if (sites[0] != 2) throw new AssertionError(sites[0]);
        final byte[] delayed = cls.toBytecode();
        final PrintWriter marker = new PrintWriter(argument, "UTF-8");
        instrumentation.addTransformer(new ClassFileTransformer() {
            public byte[] transform(ClassLoader loader, String name, Class<?> redefining, ProtectionDomain domain, byte[] bytes) {
                if (!name.equals("oracle/Recorder")) return null;
                marker.println("active");
                marker.close();
                return delayed;
            }
        });
    }
}
