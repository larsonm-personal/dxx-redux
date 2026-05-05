package com.dxxredux.app

import android.text.InputType
import org.junit.Assert.assertEquals
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
}