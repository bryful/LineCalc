package io.github.bryful.linecalc;

import android.content.Intent;
import android.test.InstrumentationTestCase;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;

public final class ScreenIntegrationTest extends InstrumentationTestCase
{
    private MainActivity activity;

    @Override protected void setUp() throws Exception
    {
        super.setUp();
        getInstrumentation().getTargetContext().getSharedPreferences("linecalc", 0)
            .edit().putBoolean("systemKeyboard", false).commit();
    }

    private CalculatorKeyboard keyboard(View view)
    {
        if (view instanceof CalculatorKeyboard) return (CalculatorKeyboard)view;
        if (view instanceof ViewGroup)
        {
            ViewGroup group = (ViewGroup)view;
            for (int i = 0; i < group.getChildCount(); ++i)
            {
                CalculatorKeyboard result = keyboard(group.getChildAt(i));
                if (result != null) return result;
            }
        }
        return null;
    }

    private void press(String label)
    {
        getInstrumentation().runOnMainSync(() ->
        {
            Button key = findButton(keyboard(activity.getWindow().getDecorView()), label);
            assertNotNull("キーが見つかりません: " + label, key);
            assertTrue(key.isEnabled());
            key.performClick();
        });
    }

    private void assertReadable(View view)
    {
        if (view instanceof Button && view.isShown())
        {
            Button button = (Button)view;
            android.text.Layout layout = button.getLayout();
            assertNotNull(button.getText().toString(), layout);
            assertEquals(button.getText().toString(), 1, layout.getLineCount());
            assertTrue("文字の高さ: " + button.getText(), layout.getLineBottom(0)
                <= button.getHeight() - button.getCompoundPaddingTop() - button.getCompoundPaddingBottom());
            assertTrue("文字の幅: " + button.getText(), layout.getLineWidth(0)
                <= button.getWidth() - button.getCompoundPaddingLeft() - button.getCompoundPaddingRight() + 1);
        }
        if (view instanceof ViewGroup)
        {
            ViewGroup group = (ViewGroup)view;
            for (int i = 0; i < group.getChildCount(); ++i) assertReadable(group.getChildAt(i));
        }
    }

    private void waitForIme(boolean visible) throws Exception
    {
        if (android.os.Build.VERSION.SDK_INT < 30) return;
        long limit = System.nanoTime() + 5_000_000_000L;
        while (System.nanoTime() < limit)
        {
            final boolean[] matches = { false };
            getInstrumentation().runOnMainSync(() ->
            {
                android.view.WindowInsets insets = activity.getWindow().getDecorView().getRootWindowInsets();
                matches[0] = insets != null && insets.isVisible(android.view.WindowInsets.Type.ime()) == visible;
            });
            if (matches[0]) return;
            Thread.sleep(100);
        }
        fail("標準キーボードの表示状態が切り替わりません: " + visible);
    }

    private Button findButton(View view, String label)
    {
        if (view instanceof Button && label.contentEquals(((Button)view).getText())) return (Button)view;
        if (view instanceof ViewGroup)
        {
            ViewGroup group = (ViewGroup)view;
            for (int i = 0; i < group.getChildCount(); ++i)
            {
                Button found = findButton(group.getChildAt(i), label);
                if (found != null) return found;
            }
        }
        return null;
    }

    private void waitForValue(String expected) throws Exception
    {
        long limit = System.nanoTime() + 5_000_000_000L;
        while (System.nanoTime() < limit)
        {
            final boolean[] matches = { false };
            getInstrumentation().runOnMainSync(() ->
            {
                EditText input = activity.findViewById(R.id.calculation);
                matches[0] = input.isEnabled() && expected.contentEquals(input.getText());
            });
            if (matches[0]) return;
            Thread.sleep(100);
        }
        fail("入力欄が期待値になりませんでした: " + expected);
    }

    public void testInputToolbarAndErrorRecovery() throws Exception
    {
        Intent intent = new Intent(getInstrumentation().getTargetContext(), MainActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        activity = (MainActivity)getInstrumentation().startActivitySync(intent);
        try
        {
            waitForValue("");
            getInstrumentation().runOnMainSync(() -> assertReadable(activity.getWindow().getDecorView()));
            getInstrumentation().runOnMainSync(() ->
            {
                assertNotNull(findButton(activity.getWindow().getDecorView(), "履歴"));
                assertNotNull(findButton(activity.getWindow().getDecorView(), "メモリ"));
                assertNotNull(findButton(activity.getWindow().getDecorView(), "関数"));
                assertNotNull(findButton(activity.getWindow().getDecorView(), "再読込"));
                EditText input = activity.findViewById(R.id.calculation);
                input.setText("(15-2)*12");
                input.onEditorAction(EditorInfo.IME_ACTION_GO);
            });
            waitForValue("156");
            getInstrumentation().runOnMainSync(() ->
            {
                EditText input = activity.findViewById(R.id.calculation);
                input.setText("ans+4");
                findButton(activity.getWindow().getDecorView(), "実行").performClick();
            });
            waitForValue("160");
            getInstrumentation().runOnMainSync(() ->
            {
                EditText input = activity.findViewById(R.id.calculation);
                input.setText("keep=23");
                input.onEditorAction(EditorInfo.IME_ACTION_GO);
            });
            waitForValue("23");
            press("英字");
            press("⇧");
            getInstrumentation().runOnMainSync(() ->
            {
                EditText input = activity.findViewById(R.id.calculation);
                input.setSelection(0, 1);
            });
            android.app.Instrumentation.ActivityMonitor monitor = getInstrumentation()
                .addMonitor(MainActivity.class.getName(), null, false);
            getInstrumentation().runOnMainSync(() -> activity.setRequestedOrientation(
                android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE));
            android.app.Activity rotated = getInstrumentation().waitForMonitorWithTimeout(monitor, 5000);
            getInstrumentation().removeMonitor(monitor);
            assertNotNull("画面回転後のActivityが作成されませんでした。", rotated);
            activity = (MainActivity)rotated;
            waitForValue("23");
            getInstrumentation().runOnMainSync(() ->
            {
                EditText input = activity.findViewById(R.id.calculation);
                assertEquals(0, input.getSelectionStart());
                assertEquals(1, input.getSelectionEnd());
                assertTrue(keyboard(activity.getWindow().getDecorView()).isUppercase());
                assertEquals(1, keyboard(activity.getWindow().getDecorView()).getMode());
                input.setText("keep+1");
                input.onEditorAction(EditorInfo.IME_ACTION_GO);
            });
            waitForValue("24");
            getInstrumentation().waitForIdleSync();
            getInstrumentation().runOnMainSync(() -> assertReadable(activity.getWindow().getDecorView()));
            press("JS");
            getInstrumentation().waitForIdleSync();
            getInstrumentation().runOnMainSync(() -> assertReadable(activity.getWindow().getDecorView()));
            press("数値");
            getInstrumentation().waitForIdleSync();
            getInstrumentation().runOnMainSync(() -> assertReadable(activity.getWindow().getDecorView()));
        }
        finally { getInstrumentation().runOnMainSync(activity::finish); }
    }

    public void testCustomKeyboardEditingAndRepeat() throws Exception
    {
        Intent intent = new Intent(getInstrumentation().getTargetContext(), MainActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        activity = (MainActivity)getInstrumentation().startActivitySync(intent);
        try
        {
            waitForValue("");
            waitForIme(false);
            getInstrumentation().runOnMainSync(() ->
            {
                EditText input = activity.findViewById(R.id.calculation);
                assertFalse(input.getShowSoftInputOnFocus());
                assertTrue(keyboard(activity.getWindow().getDecorView()).isShown());
            });
            press("1"); press("2"); press("×"); press("3");
            waitForValue("12*3");
            press("実行");
            waitForValue("36");
            press("AC"); press("1"); press("="); press("2");
            waitForValue("1=2");
            getInstrumentation().runOnMainSync(() ->
            {
                EditText input = activity.findViewById(R.id.calculation);
                input.setText("abcd");
                input.setSelection(1, 3);
            });
            press("JS"); press("===");
            waitForValue("a===d");
            press("⌫");
            waitForValue("a==d");
            press("英字"); press("⇧"); press("Q");
            waitForValue("a==Qd");
            getInstrumentation().runOnMainSync(() ->
            {
                EditText input = activity.findViewById(R.id.calculation);
                input.setText("a\uD83D\uDE00b");
                input.setSelection(3);
            });
            press("◀");
            getInstrumentation().runOnMainSync(() ->
                assertEquals(1, ((EditText)activity.findViewById(R.id.calculation)).getSelectionStart()));
            press("▶"); press("⌫");
            waitForValue("ab");
            getInstrumentation().runOnMainSync(() ->
            {
                EditText input = activity.findViewById(R.id.calculation);
                input.setText("1234567890123456789012345678901234567890");
                input.setSelection(input.length());
                Button delete = findButton(keyboard(activity.getWindow().getDecorView()), "⌫");
                long now = android.os.SystemClock.uptimeMillis();
                android.view.MotionEvent event = android.view.MotionEvent.obtain(now, now,
                    android.view.MotionEvent.ACTION_DOWN, delete.getWidth() / 2f, delete.getHeight() / 2f, 0);
                delete.dispatchTouchEvent(event);
                event.recycle();
            });
            Thread.sleep(android.view.ViewConfiguration.getLongPressTimeout() + 250);
            final int[] remaining = { 0 };
            getInstrumentation().runOnMainSync(() ->
            {
                Button delete = findButton(keyboard(activity.getWindow().getDecorView()), "⌫");
                long now = android.os.SystemClock.uptimeMillis();
                android.view.MotionEvent event = android.view.MotionEvent.obtain(now, now,
                    android.view.MotionEvent.ACTION_UP, delete.getWidth() / 2f, delete.getHeight() / 2f, 0);
                delete.dispatchTouchEvent(event);
                event.recycle();
                remaining[0] = ((EditText)activity.findViewById(R.id.calculation)).length();
                assertTrue(remaining[0] < 39);
                assertTrue(remaining[0] > 0);
            });
            Thread.sleep(200);
            getInstrumentation().runOnMainSync(() ->
                assertEquals(remaining[0], ((EditText)activity.findViewById(R.id.calculation)).length()));
            press("数値"); press("AC"); press("ans"); press("実行");
            waitForValue("36");
        }
        finally { getInstrumentation().runOnMainSync(activity::finish); }
    }

    public void testSystemKeyboardSwitch() throws Exception
    {
        Intent intent = new Intent(getInstrumentation().getTargetContext(), MainActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        activity = (MainActivity)getInstrumentation().startActivitySync(intent);
        try
        {
            waitForValue("");
            toggleFromMenu("標準キーボードに切替");
            waitForIme(true);
            getInstrumentation().runOnMainSync(() ->
            {
                assertTrue(((EditText)activity.findViewById(R.id.calculation)).getShowSoftInputOnFocus());
                assertEquals(View.GONE, keyboard(activity.getWindow().getDecorView()).getVisibility());
            });
            toggleFromMenu("専用キーボードに切替");
            waitForIme(false);
            getInstrumentation().runOnMainSync(() ->
            {
                assertFalse(((EditText)activity.findViewById(R.id.calculation)).getShowSoftInputOnFocus());
                assertTrue(keyboard(activity.getWindow().getDecorView()).isShown());
            });
        }
        finally { getInstrumentation().runOnMainSync(activity::finish); }
    }

    private void toggleFromMenu(String label) throws Exception
    {
        getInstrumentation().runOnMainSync(() ->
            findButton(activity.getWindow().getDecorView(), "⋮").performClick());
        long limit = System.nanoTime() + 3_000_000_000L;
        while (System.nanoTime() < limit)
        {
            android.view.accessibility.AccessibilityNodeInfo root = getInstrumentation().getUiAutomation().getRootInActiveWindow();
            if (root != null)
            {
                var nodes = root.findAccessibilityNodeInfosByText(label);
                if (!nodes.isEmpty())
                {
                    for (int i = 0; i < 7; ++i)
                        getInstrumentation().sendKeyDownUpSync(android.view.KeyEvent.KEYCODE_DPAD_DOWN);
                    getInstrumentation().sendKeyDownUpSync(android.view.KeyEvent.KEYCODE_DPAD_CENTER);
                    getInstrumentation().waitForIdleSync();
                    return;
                }
            }
            Thread.sleep(100);
        }
        fail("切替メニューが見つかりません: " + label);
    }
}
