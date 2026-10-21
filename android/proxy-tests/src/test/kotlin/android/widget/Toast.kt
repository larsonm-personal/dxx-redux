@file:Suppress("UNUSED_PARAMETER")

package android.widget

class Toast {
    fun show() = Unit

    companion object {
        const val LENGTH_LONG = 1

        fun makeText(
            context: android.content.Context,
            message: String,
            duration: Int,
        ) = Toast()
    }
}
