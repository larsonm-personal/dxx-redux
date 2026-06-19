package com.dxxredux.app

import android.text.InputType
import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class MainActivityInputTypeTest {
    @Test
    fun buildKeyboardEditorInputType_keepsNumericModeFreeOfTextFlags() {
        val inputType = buildKeyboardEditorInputType(InputType.TYPE_CLASS_NUMBER)

        assertEquals(InputType.TYPE_CLASS_NUMBER, inputType)
        assertEquals(0, inputType and InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS)
        assertEquals(0, inputType and InputType.TYPE_MASK_VARIATION)
    }

    @Test
    fun buildKeyboardEditorInputType_addsImmediateCommitFlagsForTextMode() {
        val inputType = buildKeyboardEditorInputType(InputType.TYPE_CLASS_TEXT)

        assertEquals(InputType.TYPE_CLASS_TEXT, inputType and InputType.TYPE_MASK_CLASS)
        assertTrue(inputType and InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS != 0)
        assertEquals(
            InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD,
            inputType and InputType.TYPE_MASK_VARIATION,
        )
    }

    @Test
    fun imeCommittedCodePointFromKeyEvent_returnsPrintableDigits() {
        assertEquals('5'.code, imeCommittedCodePointFromKeyEvent(KeyEvent.KEYCODE_5, '5'.code))
        assertEquals('0'.code, imeCommittedCodePointFromKeyEvent(KeyEvent.KEYCODE_NUMPAD_0, '0'.code))
    }

    @Test
    fun imeCommittedCodePointFromKeyEvent_ignoresSpecialKeys() {
        assertNull(imeCommittedCodePointFromKeyEvent(KeyEvent.KEYCODE_DEL, 0))
        assertNull(imeCommittedCodePointFromKeyEvent(KeyEvent.KEYCODE_ENTER, '\r'.code))
        assertNull(imeCommittedCodePointFromKeyEvent(KeyEvent.KEYCODE_NUMPAD_ENTER, '\n'.code))
    }

    @Test
    fun imeNativeSpecialKeyCode_mapsDeleteAndEnter() {
        assertEquals(KeyEvent.KEYCODE_DEL, imeNativeSpecialKeyCode(KeyEvent.KEYCODE_DEL))
        assertEquals(KeyEvent.KEYCODE_ENTER, imeNativeSpecialKeyCode(KeyEvent.KEYCODE_ENTER))
        assertEquals(KeyEvent.KEYCODE_ENTER, imeNativeSpecialKeyCode(KeyEvent.KEYCODE_NUMPAD_ENTER))
        assertNull(imeNativeSpecialKeyCode(KeyEvent.KEYCODE_7))
    }

    @Test
    fun shouldConsumeKeyboardBack_onlyWhenImeVisibleOrGamepadOnly() {
        assertTrue(
            shouldConsumeKeyboardBack(
                keyboardActive = true,
                keyboardImeVisible = true,
                gamepadOnlyMode = false,
            ),
        )
        assertTrue(
            shouldConsumeKeyboardBack(
                keyboardActive = true,
                keyboardImeVisible = false,
                gamepadOnlyMode = true,
            ),
        )
        assertFalse(
            shouldConsumeKeyboardBack(
                keyboardActive = true,
                keyboardImeVisible = false,
                gamepadOnlyMode = false,
            ),
        )
        assertFalse(
            shouldConsumeKeyboardBack(
                keyboardActive = false,
                keyboardImeVisible = true,
                gamepadOnlyMode = false,
            ),
        )
    }
}
