package oracle;

import com.badlogic.gdx.graphics.GL20;
import static oracle.Platform.*;

final class NullGL20 implements GL20 {
    @Override public String toString() { return "Oracle:GL20"; }
    @Override public boolean glIsBuffer(int a0) {
        return false;
    }
    @Override public boolean glIsEnabled(int a0) {
        return false;
    }
    @Override public boolean glIsFramebuffer(int a0) {
        return false;
    }
    @Override public boolean glIsProgram(int a0) {
        return false;
    }
    @Override public boolean glIsRenderbuffer(int a0) {
        return false;
    }
    @Override public boolean glIsShader(int a0) {
        return false;
    }
    @Override public boolean glIsTexture(int a0) {
        return false;
    }
    @Override public int glCheckFramebufferStatus(int a0) {
        return 36053;
    }
    @Override public int glCreateProgram() {
        return nextObject++;
    }
    @Override public int glCreateShader(int a0) {
        return nextObject++;
    }
    @Override public int glGenBuffer() {
        return nextObject++;
    }
    @Override public int glGenFramebuffer() {
        return nextObject++;
    }
    @Override public int glGenRenderbuffer() {
        return nextObject++;
    }
    @Override public int glGenTexture() {
        return nextObject++;
    }
    @Override public int glGetAttribLocation(int a0, java.lang.String a1) {
        return 0;
    }
    @Override public int glGetError() {
        return 0;
    }
    @Override public int glGetUniformLocation(int a0, java.lang.String a1) {
        return 0;
    }
    @Override public java.lang.String glGetActiveAttrib(int a0, int a1, java.nio.IntBuffer a2, java.nio.Buffer a3) {
        return null;
    }
    @Override public java.lang.String glGetActiveUniform(int a0, int a1, java.nio.IntBuffer a2, java.nio.Buffer a3) {
        return null;
    }
    @Override public java.lang.String glGetProgramInfoLog(int a0) {
        return "";
    }
    @Override public java.lang.String glGetShaderInfoLog(int a0) {
        return "";
    }
    @Override public java.lang.String glGetString(int a0) {
        return "2.1 Oracle null device";
    }
    @Override public void glActiveTexture(int a0) {
        activeTexture = a0;
    }
    @Override public void glAttachShader(int a0, int a1) {

    }
    @Override public void glBindAttribLocation(int a0, int a1, java.lang.String a2) {

    }
    @Override public void glBindBuffer(int a0, int a1) {

    }
    @Override public void glBindFramebuffer(int a0, int a1) {
        framebuffer = a1;
    }
    @Override public void glBindRenderbuffer(int a0, int a1) {

    }
    @Override public void glBindTexture(int a0, int a1) {
        textures.put(activeTexture, a1);
    }
    @Override public void glBlendColor(float a0, float a1, float a2, float a3) {

    }
    @Override public void glBlendEquation(int a0) {

    }
    @Override public void glBlendEquationSeparate(int a0, int a1) {

    }
    @Override public void glBlendFunc(int a0, int a1) {

    }
    @Override public void glBlendFuncSeparate(int a0, int a1, int a2, int a3) {

    }
    @Override public void glBufferData(int a0, int a1, java.nio.Buffer a2, int a3) {

    }
    @Override public void glBufferSubData(int a0, int a1, int a2, java.nio.Buffer a3) {

    }
    @Override public void glClear(int a0) {

    }
    @Override public void glClearColor(float a0, float a1, float a2, float a3) {

    }
    @Override public void glClearDepthf(float a0) {

    }
    @Override public void glClearStencil(int a0) {

    }
    @Override public void glColorMask(boolean a0, boolean a1, boolean a2, boolean a3) {

    }
    @Override public void glCompileShader(int a0) {

    }
    @Override public void glCompressedTexImage2D(int a0, int a1, int a2, int a3, int a4, int a5, int a6, java.nio.Buffer a7) {

    }
    @Override public void glCompressedTexSubImage2D(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, java.nio.Buffer a8) {

    }
    @Override public void glCopyTexImage2D(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7) {

    }
    @Override public void glCopyTexSubImage2D(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7) {

    }
    @Override public void glCullFace(int a0) {

    }
    @Override public void glDeleteBuffer(int a0) {

    }
    @Override public void glDeleteBuffers(int a0, java.nio.IntBuffer a1) {

    }
    @Override public void glDeleteFramebuffer(int a0) {

    }
    @Override public void glDeleteFramebuffers(int a0, java.nio.IntBuffer a1) {

    }
    @Override public void glDeleteProgram(int a0) {

    }
    @Override public void glDeleteRenderbuffer(int a0) {

    }
    @Override public void glDeleteRenderbuffers(int a0, java.nio.IntBuffer a1) {

    }
    @Override public void glDeleteShader(int a0) {

    }
    @Override public void glDeleteTexture(int a0) {

    }
    @Override public void glDeleteTextures(int a0, java.nio.IntBuffer a1) {

    }
    @Override public void glDepthFunc(int a0) {

    }
    @Override public void glDepthMask(boolean a0) {

    }
    @Override public void glDepthRangef(float a0, float a1) {

    }
    @Override public void glDetachShader(int a0, int a1) {

    }
    @Override public void glDisable(int a0) {

    }
    @Override public void glDisableVertexAttribArray(int a0) {

    }
    @Override public void glDrawArrays(int a0, int a1, int a2) {

    }
    @Override public void glDrawElements(int a0, int a1, int a2, int a3) {

    }
    @Override public void glDrawElements(int a0, int a1, int a2, java.nio.Buffer a3) {

    }
    @Override public void glEnable(int a0) {

    }
    @Override public void glEnableVertexAttribArray(int a0) {

    }
    @Override public void glFinish() {

    }
    @Override public void glFlush() {

    }
    @Override public void glFramebufferRenderbuffer(int a0, int a1, int a2, int a3) {

    }
    @Override public void glFramebufferTexture2D(int a0, int a1, int a2, int a3, int a4) {

    }
    @Override public void glFrontFace(int a0) {

    }
    @Override public void glGenBuffers(int a0, java.nio.IntBuffer a1) {
        for (int i = 0; i < a0; i++) a1.put(a1.position() + i, nextObject++);
    }
    @Override public void glGenFramebuffers(int a0, java.nio.IntBuffer a1) {
        for (int i = 0; i < a0; i++) a1.put(a1.position() + i, nextObject++);
    }
    @Override public void glGenRenderbuffers(int a0, java.nio.IntBuffer a1) {
        for (int i = 0; i < a0; i++) a1.put(a1.position() + i, nextObject++);
    }
    @Override public void glGenTextures(int a0, java.nio.IntBuffer a1) {
        for (int i = 0; i < a0; i++) a1.put(a1.position() + i, nextObject++);
    }
    @Override public void glGenerateMipmap(int a0) {

    }
    @Override public void glGetAttachedShaders(int a0, int a1, java.nio.Buffer a2, java.nio.IntBuffer a3) {

    }
    @Override public void glGetBooleanv(int a0, java.nio.Buffer a1) {

    }
    @Override public void glGetBufferParameteriv(int a0, int a1, java.nio.IntBuffer a2) {

    }
    @Override public void glGetFloatv(int a0, java.nio.FloatBuffer a1) {

    }
    @Override public void glGetFramebufferAttachmentParameteriv(int a0, int a1, int a2, java.nio.IntBuffer a3) {

    }
    @Override public void glGetIntegerv(int a0, java.nio.IntBuffer a1) {
        int at = a1.position();
        if (a0 == 2978) for (int i = 0; i < 4; i++) a1.put(at + i, viewport[i]);
        else a1.put(at, a0 == 36006 ? framebuffer : a0 == 34016 ? activeTexture
            : a0 == 32873 ? textures.getOrDefault(activeTexture, 0)
            : a0 == 3379 || a0 == 34024 ? 16384 : a0 == 34930 ? 16 : 0);
    }
    @Override public void glGetProgramiv(int a0, int a1, java.nio.IntBuffer a2) {
        a2.put(0, a1 == 35713 || a1 == 35714 ? 1 : 0);
    }
    @Override public void glGetRenderbufferParameteriv(int a0, int a1, java.nio.IntBuffer a2) {

    }
    @Override public void glGetShaderPrecisionFormat(int a0, int a1, java.nio.IntBuffer a2, java.nio.IntBuffer a3) {

    }
    @Override public void glGetShaderiv(int a0, int a1, java.nio.IntBuffer a2) {
        a2.put(0, a1 == 35713 || a1 == 35714 ? 1 : 0);
    }
    @Override public void glGetTexParameterfv(int a0, int a1, java.nio.FloatBuffer a2) {

    }
    @Override public void glGetTexParameteriv(int a0, int a1, java.nio.IntBuffer a2) {

    }
    @Override public void glGetUniformfv(int a0, int a1, java.nio.FloatBuffer a2) {

    }
    @Override public void glGetUniformiv(int a0, int a1, java.nio.IntBuffer a2) {

    }
    @Override public void glGetVertexAttribPointerv(int a0, int a1, java.nio.Buffer a2) {

    }
    @Override public void glGetVertexAttribfv(int a0, int a1, java.nio.FloatBuffer a2) {

    }
    @Override public void glGetVertexAttribiv(int a0, int a1, java.nio.IntBuffer a2) {

    }
    @Override public void glHint(int a0, int a1) {

    }
    @Override public void glLineWidth(float a0) {

    }
    @Override public void glLinkProgram(int a0) {

    }
    @Override public void glPixelStorei(int a0, int a1) {

    }
    @Override public void glPolygonOffset(float a0, float a1) {

    }
    @Override public void glReadPixels(int a0, int a1, int a2, int a3, int a4, int a5, java.nio.Buffer a6) {

    }
    @Override public void glReleaseShaderCompiler() {

    }
    @Override public void glRenderbufferStorage(int a0, int a1, int a2, int a3) {

    }
    @Override public void glSampleCoverage(float a0, boolean a1) {

    }
    @Override public void glScissor(int a0, int a1, int a2, int a3) {

    }
    @Override public void glShaderBinary(int a0, java.nio.IntBuffer a1, int a2, java.nio.Buffer a3, int a4) {

    }
    @Override public void glShaderSource(int a0, java.lang.String a1) {

    }
    @Override public void glStencilFunc(int a0, int a1, int a2) {

    }
    @Override public void glStencilFuncSeparate(int a0, int a1, int a2, int a3) {

    }
    @Override public void glStencilMask(int a0) {

    }
    @Override public void glStencilMaskSeparate(int a0, int a1) {

    }
    @Override public void glStencilOp(int a0, int a1, int a2) {

    }
    @Override public void glStencilOpSeparate(int a0, int a1, int a2, int a3) {

    }
    @Override public void glTexImage2D(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, java.nio.Buffer a8) {

    }
    @Override public void glTexParameterf(int a0, int a1, float a2) {

    }
    @Override public void glTexParameterfv(int a0, int a1, java.nio.FloatBuffer a2) {

    }
    @Override public void glTexParameteri(int a0, int a1, int a2) {

    }
    @Override public void glTexParameteriv(int a0, int a1, java.nio.IntBuffer a2) {

    }
    @Override public void glTexSubImage2D(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, java.nio.Buffer a8) {

    }
    @Override public void glUniform1f(int a0, float a1) {

    }
    @Override public void glUniform1fv(int a0, int a1, float[] a2, int a3) {

    }
    @Override public void glUniform1fv(int a0, int a1, java.nio.FloatBuffer a2) {

    }
    @Override public void glUniform1i(int a0, int a1) {

    }
    @Override public void glUniform1iv(int a0, int a1, int[] a2, int a3) {

    }
    @Override public void glUniform1iv(int a0, int a1, java.nio.IntBuffer a2) {

    }
    @Override public void glUniform2f(int a0, float a1, float a2) {

    }
    @Override public void glUniform2fv(int a0, int a1, float[] a2, int a3) {

    }
    @Override public void glUniform2fv(int a0, int a1, java.nio.FloatBuffer a2) {

    }
    @Override public void glUniform2i(int a0, int a1, int a2) {

    }
    @Override public void glUniform2iv(int a0, int a1, int[] a2, int a3) {

    }
    @Override public void glUniform2iv(int a0, int a1, java.nio.IntBuffer a2) {

    }
    @Override public void glUniform3f(int a0, float a1, float a2, float a3) {

    }
    @Override public void glUniform3fv(int a0, int a1, float[] a2, int a3) {

    }
    @Override public void glUniform3fv(int a0, int a1, java.nio.FloatBuffer a2) {

    }
    @Override public void glUniform3i(int a0, int a1, int a2, int a3) {

    }
    @Override public void glUniform3iv(int a0, int a1, int[] a2, int a3) {

    }
    @Override public void glUniform3iv(int a0, int a1, java.nio.IntBuffer a2) {

    }
    @Override public void glUniform4f(int a0, float a1, float a2, float a3, float a4) {

    }
    @Override public void glUniform4fv(int a0, int a1, float[] a2, int a3) {

    }
    @Override public void glUniform4fv(int a0, int a1, java.nio.FloatBuffer a2) {

    }
    @Override public void glUniform4i(int a0, int a1, int a2, int a3, int a4) {

    }
    @Override public void glUniform4iv(int a0, int a1, int[] a2, int a3) {

    }
    @Override public void glUniform4iv(int a0, int a1, java.nio.IntBuffer a2) {

    }
    @Override public void glUniformMatrix2fv(int a0, int a1, boolean a2, float[] a3, int a4) {

    }
    @Override public void glUniformMatrix2fv(int a0, int a1, boolean a2, java.nio.FloatBuffer a3) {

    }
    @Override public void glUniformMatrix3fv(int a0, int a1, boolean a2, float[] a3, int a4) {

    }
    @Override public void glUniformMatrix3fv(int a0, int a1, boolean a2, java.nio.FloatBuffer a3) {

    }
    @Override public void glUniformMatrix4fv(int a0, int a1, boolean a2, float[] a3, int a4) {

    }
    @Override public void glUniformMatrix4fv(int a0, int a1, boolean a2, java.nio.FloatBuffer a3) {

    }
    @Override public void glUseProgram(int a0) {

    }
    @Override public void glValidateProgram(int a0) {

    }
    @Override public void glVertexAttrib1f(int a0, float a1) {

    }
    @Override public void glVertexAttrib1fv(int a0, java.nio.FloatBuffer a1) {

    }
    @Override public void glVertexAttrib2f(int a0, float a1, float a2) {

    }
    @Override public void glVertexAttrib2fv(int a0, java.nio.FloatBuffer a1) {

    }
    @Override public void glVertexAttrib3f(int a0, float a1, float a2, float a3) {

    }
    @Override public void glVertexAttrib3fv(int a0, java.nio.FloatBuffer a1) {

    }
    @Override public void glVertexAttrib4f(int a0, float a1, float a2, float a3, float a4) {

    }
    @Override public void glVertexAttrib4fv(int a0, java.nio.FloatBuffer a1) {

    }
    @Override public void glVertexAttribPointer(int a0, int a1, int a2, boolean a3, int a4, int a5) {

    }
    @Override public void glVertexAttribPointer(int a0, int a1, int a2, boolean a3, int a4, java.nio.Buffer a5) {

    }
    @Override public void glViewport(int a0, int a1, int a2, int a3) {
        viewport[0] = a0; viewport[1] = a1; viewport[2] = a2; viewport[3] = a3;
    }
}
