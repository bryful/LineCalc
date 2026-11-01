package io.github.bryful.linecalc;

final class NativeCalculator
{
    static { System.loadLibrary("linecalc"); }
    private NativeCalculator() {}
    static native long create(byte[] acorn, byte[] runtime, byte[] path);
    static native String[] execute(long handle, byte[] source, int action, int memory);
    static native String[] memories(long handle);
    static native void destroy(long handle);
}
