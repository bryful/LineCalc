package io.github.bryful.linecalc;

import android.test.InstrumentationTestCase;
import java.nio.charset.StandardCharsets;

public final class CalculatorIntegrationTest extends InstrumentationTestCase
{
    private CalculatorSession session;

    @Override protected void setUp() throws Exception
    {
        super.setUp();
        session = new CalculatorSession(getInstrumentation().getTargetContext());
        session.execute("", 1, -1);
    }

    @Override protected void tearDown() throws Exception
    {
        session.close();
        super.tearDown();
    }

    private String[] run(String source) throws Exception { return session.execute(source, 0, -1); }

    private void value(String source, String expected) throws Exception
    {
        String[] result = run(source);
        assertEquals(result[2], "1", result[0]);
        assertEquals(expected, result[1]);
    }

    public void testCalculationsAndMemory() throws Exception
    {
        value("(15-2)*12", "156");
        value("ans+1", "157");
        value("var a", "");
        value("a", "0");
        value("a=0;for(var i=0;i<10;i++) a+=i;a;", "45");
        value("hex(255)", "0xff");
        value("function 三倍(v){return v*3;}", "");
        value("三倍(5)", "15");
        assertEquals("1", session.execute("-42", 3, 0)[0]);
        assertEquals("(-42)", session.memories()[0]);
        value("M1+2", "-40");
        assertEquals("0", run("1/0")[0]);
        assertEquals("0", run("setTopmost(true)")[0]);
        assertEquals("0", run("a='text'")[0]);
    }

    public void testAutomaticDefinitionsAndReload() throws Exception
    {
        String original = session.store.read();
        android.net.Uri file = session.store.file();
        try
        {
            try (var output = getInstrumentation().getTargetContext().getContentResolver().openOutputStream(file, "wt"))
            {
                assertNotNull(output);
                output.write("function triple(v){return v*3;}".getBytes(StandardCharsets.UTF_8));
            }
            value("resetJS()", "");
            value("triple(7)", "21");
            assertTrue(session.store.read().contains("triple"));
            assertEquals("1", session.execute("", 2, -1)[0]);
            value("triple(8)", "24");
            try (var output = getInstrumentation().getTargetContext().getContentResolver().openOutputStream(file, "wt"))
            {
                output.write("function broken(".getBytes(StandardCharsets.UTF_8));
            }
            assertEquals("0", run("resetJS()")[0]);
            value("2+2", "4");
        }
        finally
        {
            try (var output = getInstrumentation().getTargetContext().getContentResolver().openOutputStream(file, "wt"))
            {
                output.write(original.getBytes(StandardCharsets.UTF_8));
            }
        }
    }

    public void testTimeoutRecoveryAndValidationIsolation() throws Exception
    {
        long start = System.nanoTime();
        assertEquals("0", run("while(true){}")[0]);
        assertTrue((System.nanoTime() - start) / 1_000_000 < 5000);
        value("keep=27", "27");
        assertEquals("1", session.execute("", 2, -1)[0]);
        value("keep", "27");
        value("2+3", "5");
    }
}
