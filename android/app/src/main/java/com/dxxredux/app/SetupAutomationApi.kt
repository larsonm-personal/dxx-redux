package com.dxxredux.app

import android.content.Context
import android.graphics.Rect
import android.os.SystemClock
import android.util.Log
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityNodeInfo
import android.view.inputmethod.InputMethodManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.jsonPrimitive
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Walk the Compose accessibility node provider to discover all interactive
 * elements (buttons, chips, checkboxes) with their text, enabled state, and
 * screen-pixel bounds. Zero annotation required -- Compose auto-generates
 * accessibility nodes for every semantics-bearing composable.
 *
 * Compose assigns sequential integer IDs to semantics nodes starting from 1.
 * We scan a range and collect nodes that are clickable or checkable with
 * non-empty text and non-zero bounds.
 */
internal fun SetupActivity.collectAccessibleButtons(): List<SetupActivity.ButtonInfo> {
    val root = window.decorView
    val composeView = findComposeView(root)
    if (composeView == null) {
        Log.w("DXX-Buttons", "No ComposeView found in view tree")
        return emptyList()
    }
    val provider = composeView.accessibilityNodeProvider
    if (provider == null) {
        Log.w("DXX-Buttons", "ComposeView has no accessibility node provider")
        return emptyList()
    }

    data class TextNode(
        val text: String,
        val focused: Boolean,
        val bounds: Rect,
    )

    data class ClickableNode(
        val enabled: Boolean,
        val focused: Boolean,
        val bounds: Rect,
    )

    val textNodes = mutableListOf<TextNode>()
    val clickableNodes = mutableListOf<ClickableNode>()

    val semanticsIds = collectSemanticsNodeIds(composeView)
    val maxScanId = accessibilityScanMaxId(semanticsIds)
    for (id in -1..maxScanId) {
        val info = provider.createAccessibilityNodeInfo(id) ?: continue
        val bounds = Rect()
        info.getBoundsInScreen(bounds)
        if (bounds.width() > 0 && bounds.height() > 0) {
            val focused = info.isFocused || info.isAccessibilityFocused
            info.text?.toString()?.let { t ->
                if (t.isNotEmpty()) textNodes.add(TextNode(t, focused, Rect(bounds)))
            }
            if (info.isClickable || info.isCheckable) {
                clickableNodes.add(
                    ClickableNode(
                        enabled = info.isEnabled,
                        focused = focused,
                        bounds = Rect(bounds),
                    ),
                )
            }
        }
    }

    val buttons =
        clickableNodes.mapNotNull { click ->
            val contained = textNodes.filter { textBelongsToClickable(it.bounds, click.bounds) }
            if (contained.isEmpty()) return@mapNotNull null
            val label = contained.joinToString(" ") { it.text }
            SetupActivity.ButtonInfo(
                text = label,
                enabled = click.enabled,
                focused = click.focused || contained.any { it.focused },
                centerX = (click.bounds.left + click.bounds.right) / 2f,
                centerY = (click.bounds.top + click.bounds.bottom) / 2f,
                width = click.bounds.width().toFloat(),
                height = click.bounds.height().toFloat(),
            )
        }
    if (buttons.isEmpty() && textNodes.isNotEmpty() && clickableNodes.isNotEmpty()) {
        Log.w(
            "DXX-Buttons",
            "No button labels matched: text=${textNodes.take(5).joinToString { "${it.text}:${it.bounds}" }} " +
                "clickable=${clickableNodes.take(5).joinToString { it.bounds.toString() }}",
        )
    }
    Log.i(
        "DXX-Buttons",
        "Scan: ${textNodes.size} text, ${clickableNodes.size} clickable, " +
            "${buttons.size} matched, range=-1..$maxScanId (semantics=${semanticsIds.size})",
    )
    return buttons
}

private fun textBelongsToClickable(
    textBounds: Rect,
    clickBounds: Rect,
): Boolean {
    val centerX = (textBounds.left + textBounds.right) / 2
    val centerY = (textBounds.top + textBounds.bottom) / 2
    return clickBounds.contains(textBounds) || clickBounds.contains(centerX, centerY)
}

internal fun largestScrollNodeIds(scrollNodes: List<Pair<Int, Int>>): List<Int> {
    val largestArea = scrollNodes.maxOfOrNull { it.second } ?: return emptyList()
    if (largestArea <= 0) return emptyList()
    return scrollNodes.filter { it.second == largestArea }.map { it.first }
}

internal fun scrollGestureXFractions(isLandscape: Boolean): List<Float> =
    if (isLandscape) listOf(0.25f, 0.75f) else listOf(0.5f)

internal fun accessibilityScanMaxId(semanticsIds: Set<Int>): Int = semanticsIds.maxOrNull()?.plus(500) ?: 16383

internal fun accessibilityScrollScanMaxId(semanticsIds: Set<Int>): Int =
    maxOf(accessibilityScanMaxId(semanticsIds), 16383)

private fun findComposeView(view: View): View? {
    if (view.accessibilityNodeProvider != null &&
        view.javaClass.simpleName.contains("Compose")
    ) {
        return view
    }
    if (view is ViewGroup) {
        for (i in 0 until view.childCount) {
            val result = findComposeView(view.getChildAt(i))
            if (result != null) return result
        }
    }
    return null
}

// Collect all semantics node IDs from the Compose semantic tree via
// reflection on Jetpack library classes (not restricted by hidden API).
// This discovers LazyColumn items that sequential ID scanning misses,
// because lazy items can have IDs well above the 0..16383 scan range.
private fun collectSemanticsNodeIds(composeView: View): Set<Int> {
    return try {
        val getOwner = composeView.javaClass.getMethod("getSemanticsOwner")
        val owner = getOwner.invoke(composeView) ?: return emptySet()
        val getRoot = owner.javaClass.getMethod("getRootSemanticsNode")
        val rootNode = getRoot.invoke(owner) ?: return emptySet()
        val getId = rootNode.javaClass.getMethod("getId")
        val getChildren = rootNode.javaClass.getMethod("getChildren")
        val ids = mutableSetOf<Int>()
        val stack = ArrayDeque<Any>()
        stack.add(rootNode)
        while (stack.isNotEmpty()) {
            val node = stack.removeFirst()
            ids.add(getId.invoke(node) as Int)
            @Suppress("UNCHECKED_CAST")
            val children = getChildren.invoke(node) as? List<Any> ?: emptyList()
            stack.addAll(children)
        }
        ids
    } catch (e: Exception) {
        Log.w("DXX-Buttons", "collectSemanticsNodeIds: ${e.message}")
        emptySet()
    }
}

/** Find a button by case-insensitive text match, optionally exact-only. */
internal fun SetupActivity.findButtonByText(
    text: String,
    exactOnly: Boolean = false,
): SetupActivity.ButtonInfo? {
    val buttons = collectAccessibleButtons()
    val lower = text.lowercase()
    return buttons.find { it.text.lowercase() == lower }
        ?: if (exactOnly) null else buttons.find { it.text.lowercase().contains(lower) }
}

/** Inject a real tap (ACTION_DOWN + delay + ACTION_UP) at screen coordinates. */
internal suspend fun SetupActivity.injectTapAt(
    screenX: Float,
    screenY: Float,
) {
    withContext(Dispatchers.Main) {
        val decorView = window.decorView
        val loc = IntArray(2)
        decorView.getLocationOnScreen(loc)
        val localX = screenX - loc[0]
        val localY = screenY - loc[1]
        val downTime = SystemClock.uptimeMillis()
        val down =
            MotionEvent.obtain(
                downTime,
                downTime,
                MotionEvent.ACTION_DOWN,
                localX,
                localY,
                0,
            )
        decorView.dispatchTouchEvent(down)
        down.recycle()
        delay(50)
        val upTime = SystemClock.uptimeMillis()
        val up =
            MotionEvent.obtain(
                downTime,
                upTime,
                MotionEvent.ACTION_UP,
                localX,
                localY,
                0,
            )
        decorView.dispatchTouchEvent(up)
        up.recycle()
    }
}

/**
 * Perform a click on a Compose button via its accessibility node.
 * More reliable than touch injection because it bypasses coordinate
 * mapping and uses the semantic click action directly.
 * Returns true if the click was performed.
 */
internal fun SetupActivity.performAccessibilityClick(buttonText: String): Boolean {
    val root = window.decorView
    val composeView = findComposeView(root) ?: return false
    val provider = composeView.accessibilityNodeProvider ?: return false

    data class TextNode(
        val text: String,
        val bounds: Rect,
    )

    data class ClickNode(
        val id: Int,
        val bounds: Rect,
    )

    val textNodes = mutableListOf<TextNode>()
    val clickNodes = mutableListOf<ClickNode>()

    val semanticsIds = collectSemanticsNodeIds(composeView)
    val maxScanId = accessibilityScanMaxId(semanticsIds)
    for (id in -1..maxScanId) {
        val info = provider.createAccessibilityNodeInfo(id) ?: continue
        val bounds = Rect()
        info.getBoundsInScreen(bounds)
        if (bounds.width() > 0 && bounds.height() > 0) {
            info.text?.toString()?.let { t ->
                if (t.isNotEmpty()) textNodes.add(TextNode(t, Rect(bounds)))
            }
            if (info.isClickable) clickNodes.add(ClickNode(id, Rect(bounds)))
        }
    }

    val lower = buttonText.lowercase()
    for (click in clickNodes) {
        val contained = textNodes.filter { click.bounds.contains(it.bounds) }
        val label = contained.joinToString(" ") { it.text }
        if (label.lowercase() == lower || label.lowercase().contains(lower)) {
            return provider.performAction(
                click.id,
                AccessibilityNodeInfo.ACTION_CLICK,
                null,
            )
        }
    }
    return false
}

/** Hide the soft keyboard if it's showing. */
internal fun SetupActivity.dismissKeyboard() {
    val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
    val focused = currentFocus ?: window.decorView
    imm?.hideSoftInputFromWindow(focused.windowToken, 0)
    focused.clearFocus()
}

/** Inject a scroll-down swipe gesture (finger moves upward to scroll content down). */
internal suspend fun SetupActivity.scrollDown(): Boolean = scrollSetupContent(forward = true)

/** Inject a scroll-up swipe gesture (finger moves downward to scroll content up). */
internal suspend fun SetupActivity.scrollUp(): Boolean = scrollSetupContent(forward = false)

internal suspend fun SetupActivity.scrollToTop() {
    repeat(12) {
        if (!scrollUp()) return
    }
}

private suspend fun SetupActivity.scrollSetupContent(forward: Boolean): Boolean =
    withContext(Dispatchers.Main) {
        val isLandscape =
            resources.configuration.orientation ==
                android.content.res.Configuration.ORIENTATION_LANDSCAPE
        val root = window.decorView
        val composeView = findComposeView(root)
        val provider = composeView?.accessibilityNodeProvider
        if (composeView != null && provider != null) {
            val semanticsIds = collectSemanticsNodeIds(composeView)
            val maxScanId = accessibilityScrollScanMaxId(semanticsIds)
            val scrollNodes = mutableListOf<Pair<Int, Int>>()
            val semanticAction =
                if (forward) {
                    AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_DOWN.id
                } else {
                    AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_UP.id
                }
            val legacyAction =
                if (forward) {
                    AccessibilityNodeInfo.ACTION_SCROLL_FORWARD
                } else {
                    AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD
                }
            val oppositeSemanticAction =
                if (forward) {
                    AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_UP.id
                } else {
                    AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_DOWN.id
                }
            val oppositeLegacyAction =
                if (forward) {
                    AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD
                } else {
                    AccessibilityNodeInfo.ACTION_SCROLL_FORWARD
                }
            val oppositeScrollNodes = mutableListOf<Pair<Int, Int>>()
            for (id in -1..maxScanId) {
                val info = provider.createAccessibilityNodeInfo(id) ?: continue
                val bounds = Rect()
                info.getBoundsInScreen(bounds)
                val area = bounds.width() * bounds.height()
                when {
                    info.actionList.any { it.id == legacyAction || it.id == semanticAction } -> {
                        scrollNodes.add(id to area)
                    }

                    info.actionList.any {
                        it.id == oppositeLegacyAction || it.id == oppositeSemanticAction
                    } -> {
                        oppositeScrollNodes.add(id to area)
                    }
                }
            }
            val requestedNodeIds = largestScrollNodeIds(scrollNodes)
            var scrolled = false
            for (scrollNodeId in requestedNodeIds) {
                scrolled =
                    provider.performAction(
                        scrollNodeId,
                        legacyAction,
                        null,
                    ) ||
                    provider.performAction(
                        scrollNodeId,
                        semanticAction,
                        null,
                    ) ||
                    scrolled
            }
            if (requestedNodeIds.isNotEmpty()) {
                Log.i(
                    "DXX-Buttons",
                    "Scroll ${if (forward) "down" else "up"} nodes=${requestedNodeIds.joinToString(
                        ",",
                    )} scrolled=$scrolled",
                )
            }
            if (scrolled) {
                delay(250)
                if (!isLandscape) return@withContext true
            }
            val boundaryNodeIds = largestScrollNodeIds(oppositeScrollNodes)
            if (requestedNodeIds.isEmpty() && boundaryNodeIds.isNotEmpty()) {
                Log.i(
                    "DXX-Buttons",
                    "Scroll ${if (forward) "down" else "up"} boundary nodes=${boundaryNodeIds.joinToString(",")}",
                )
                return@withContext false
            }
        }

        val decorView = window.decorView
        val startY = decorView.height * (if (forward) 0.62f else 0.44f)
        val endY = decorView.height * (if (forward) 0.44f else 0.62f)
        for (xFraction in scrollGestureXFractions(isLandscape)) {
            val centerX = decorView.width * xFraction
            val downTime = SystemClock.uptimeMillis()
            val yFractions = listOf(0f, 0.33f, 0.66f, 1f)
            for ((index, yFraction) in yFractions.withIndex()) {
                val action =
                    when (index) {
                        0 -> MotionEvent.ACTION_DOWN
                        yFractions.lastIndex -> MotionEvent.ACTION_UP
                        else -> MotionEvent.ACTION_MOVE
                    }
                val event =
                    MotionEvent.obtain(
                        downTime,
                        SystemClock.uptimeMillis(),
                        action,
                        centerX,
                        startY + ((endY - startY) * yFraction),
                        0,
                    )
                decorView.dispatchTouchEvent(event)
                event.recycle()
                delay(50)
            }
        }
        true
    }

/**
 * Read controller_config.json and patch all .plr files with its KeySettings.
 * Patches both D1 and D2 pilots using game-specific byte arrays.
 * Returns the number of files patched.
 */
internal fun SetupActivity.patchPilotsFromConfig(): Int {
    val cfg = File(filesDir, "controller_config.json")
    var d1Patched = 0
    var d2Patched = 0
    if (!cfg.exists()) {
        writeControllerOperationResult("controller_patch_result.json", d1Patched, d2Patched)
        return 0
    }
    try {
        val json = JSONObject(cfg.readText())
        val kbArr =
            json.optJSONArray("key_settings_keyboard")
                ?: run {
                    writeControllerOperationResult("controller_patch_result.json", d1Patched, d2Patched)
                    return 0
                }
        val kb = ByteArray(kbArr.length()) { (kbArr.getInt(it) and 0xFF).toByte() }
        val ct = json.optInt("control_type", 1)
        for (game in arrayOf("d2", "d1")) {
            val joyKey = "key_settings_joystick_$game"
            val jArr = json.optJSONArray(joyKey) ?: continue
            val joy = ByteArray(jArr.length()) { (jArr.getInt(it) and 0xFF).toByte() }
            val patched =
                NativePilotPatcher.nativePatchPilotFiles(
                    filesDir.absolutePath,
                    joy,
                    kb,
                    ct,
                    game,
                )
            if (game == "d1") d1Patched = patched else d2Patched = patched
        }
        writeControllerOperationResult("controller_patch_result.json", d1Patched, d2Patched)
        return d1Patched + d2Patched
    } catch (e: Exception) {
        Log.e("DXX-Setup", "patchPilotsFromConfig failed", e)
        writeControllerOperationResult("controller_patch_result.json", d1Patched, d2Patched)
        return 0
    }
}

internal fun SetupActivity.writeControllerOperationResult(
    filename: String,
    d1Count: Int,
    d2Count: Int,
) {
    val result =
        JSONObject()
            .put("d1", d1Count)
            .put("d2", d2Count)
            .put("total", d1Count + d2Count)
    File(filesDir, filename).writeText(result.toString(2))
}

/** Write a distinct controller layout used by the real-pilot JNI roundtrip test. */
internal fun SetupActivity.writeControllerPatchFixture(game: String) {
    val bindings = loadDefaultBindings(applicationContext).toMutableMap()
    bindings["RS_X"] = "Slide L/R"
    bindings["LS_X"] = "Turn L/R"
    saveConfig(applicationContext, bindings, setOf("RS_X"), game)
}

private data class KcMeta(
    val name: String,
    val type: String,
)

// D2: 56 entries matching d2/main/kconfig.c kc_joystick[]
private val KC_JOY_META_D2 =
    listOf(
        KcMeta("Fire primary", "joy_button"), //  0
        KcMeta("Fire secondary", "joy_button"), //  1
        KcMeta("Accelerate", "joy_button"), //  2
        KcMeta("reverse", "joy_button"), //  3
        KcMeta("Fire flare", "joy_button"), //  4
        KcMeta("Slide on", "joy_button"), //  5
        KcMeta("Slide left", "joy_button"), //  6
        KcMeta("Slide right", "joy_button"), //  7
        KcMeta("Slide up", "joy_button"), //  8
        KcMeta("Slide down", "joy_button"), //  9
        KcMeta("Bank on", "joy_button"), // 10
        KcMeta("Bank left", "joy_button"), // 11
        KcMeta("Bank right", "joy_button"), // 12
        KcMeta("Pitch U/D", "joy_axis"), // 13
        KcMeta("Pitch U/D", "invert"), // 14
        KcMeta("Turn L/R", "joy_axis"), // 15
        KcMeta("Turn L/R", "invert"), // 16
        KcMeta("Slide L/R", "joy_axis"), // 17
        KcMeta("Slide L/R", "invert"), // 18
        KcMeta("Slide U/D", "joy_axis"), // 19
        KcMeta("Slide U/D", "invert"), // 20
        KcMeta("Bank L/R", "joy_axis"), // 21
        KcMeta("Bank L/R", "invert"), // 22
        KcMeta("throttle", "joy_axis"), // 23
        KcMeta("throttle", "invert"), // 24
        KcMeta("REAR VIEW", "joy_button"), // 25
        KcMeta("Drop Bomb", "joy_button"), // 26
        KcMeta("Afterburner", "joy_button"), // 27
        KcMeta("Cycle Primary", "joy_button"), // 28
        KcMeta("Cycle Secondary", "joy_button"), // 29
        KcMeta("Headlight", "joy_button"), // 30
        KcMeta("Fire primary", "joy_button"), // 31 (secondary)
        KcMeta("Fire secondary", "joy_button"), // 32
        KcMeta("Accelerate", "joy_button"), // 33
        KcMeta("reverse", "joy_button"), // 34
        KcMeta("Fire flare", "joy_button"), // 35
        KcMeta("Slide on", "joy_button"), // 36
        KcMeta("Slide left", "joy_button"), // 37
        KcMeta("Slide right", "joy_button"), // 38
        KcMeta("Slide up", "joy_button"), // 39
        KcMeta("Slide down", "joy_button"), // 40
        KcMeta("Bank on", "joy_button"), // 41
        KcMeta("Bank left", "joy_button"), // 42
        KcMeta("Bank right", "joy_button"), // 43
        KcMeta("REAR VIEW", "joy_button"), // 44
        KcMeta("Drop Bomb", "joy_button"), // 45
        KcMeta("Afterburner", "joy_button"), // 46
        KcMeta("Cycle Primary", "joy_button"), // 47
        KcMeta("Cycle Secondary", "joy_button"), // 48
        KcMeta("Headlight", "joy_button"), // 49
        KcMeta("Automap", "joy_button"), // 50
        KcMeta("Automap", "joy_button"), // 51 (secondary)
        KcMeta("Energy->Shield", "joy_button"), // 52
        KcMeta("Energy->Shield", "joy_button"), // 53 (secondary)
        KcMeta("Toggle Bomb", "joy_button"), // 54
        KcMeta("Toggle Bomb", "joy_button"), // 55 (secondary)
    )

// D1: 48 entries matching d1/main/kconfig.c kc_joystick[]
// Key differences from D2: no Afterburner/Headlight/Energy->Shield/Toggle Bomb;
// Automap at 27-28 (not 50-51); Cycle Primary/Secondary at 44-47 (not 28-29);
// different capitalization on several names.
private val KC_JOY_META_D1 =
    listOf(
        KcMeta("Fire primary", "joy_button"), //  0
        KcMeta("Fire secondary", "joy_button"), //  1
        KcMeta("Accelerate", "joy_button"), //  2
        KcMeta("Reverse", "joy_button"), //  3
        KcMeta("Fire flare", "joy_button"), //  4
        KcMeta("Slide on", "joy_button"), //  5
        KcMeta("Slide left", "joy_button"), //  6
        KcMeta("Slide right", "joy_button"), //  7
        KcMeta("Slide up", "joy_button"), //  8
        KcMeta("Slide down", "joy_button"), //  9
        KcMeta("Bank on", "joy_button"), // 10
        KcMeta("Bank left", "joy_button"), // 11
        KcMeta("Bank right", "joy_button"), // 12
        KcMeta("Pitch U/D", "joy_axis"), // 13
        KcMeta("Pitch U/D", "invert"), // 14
        KcMeta("Turn L/R", "joy_axis"), // 15
        KcMeta("Turn L/R", "invert"), // 16
        KcMeta("Slide L/R", "joy_axis"), // 17
        KcMeta("Slide L/R", "invert"), // 18
        KcMeta("Slide U/D", "joy_axis"), // 19
        KcMeta("Slide U/D", "invert"), // 20
        KcMeta("Bank L/R", "joy_axis"), // 21
        KcMeta("Bank L/R", "invert"), // 22
        KcMeta("Throttle", "joy_axis"), // 23
        KcMeta("Throttle", "invert"), // 24
        KcMeta("Rear view", "joy_button"), // 25
        KcMeta("Drop bomb", "joy_button"), // 26
        KcMeta("Automap", "joy_button"), // 27
        KcMeta("Automap", "joy_button"), // 28 (secondary)
        KcMeta("Fire primary", "joy_button"), // 29 (secondary)
        KcMeta("Fire secondary", "joy_button"), // 30
        KcMeta("Accelerate", "joy_button"), // 31
        KcMeta("Reverse", "joy_button"), // 32
        KcMeta("Fire flare", "joy_button"), // 33
        KcMeta("Slide on", "joy_button"), // 34
        KcMeta("Slide left", "joy_button"), // 35
        KcMeta("Slide right", "joy_button"), // 36
        KcMeta("Slide up", "joy_button"), // 37
        KcMeta("Slide down", "joy_button"), // 38
        KcMeta("Bank on", "joy_button"), // 39
        KcMeta("Bank left", "joy_button"), // 40
        KcMeta("Bank right", "joy_button"), // 41
        KcMeta("Rear view", "joy_button"), // 42 (secondary)
        KcMeta("Drop bomb", "joy_button"), // 43
        KcMeta("Cycle Primary", "joy_button"), // 44
        KcMeta("Cycle Secondary", "joy_button"), // 45
        KcMeta("Cycle Primary", "joy_button"), // 46 (secondary)
        KcMeta("Cycle Secondary", "joy_button"), // 47 (secondary)
    )

/**
 * Write controller_introspect.json in the same format as the in-game
 * joystick_controls introspection, but using the launcher's config.
 *
 *   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command controller_introspect --es game d2
 *   adb shell run-as com.dxxredux.app cat files/controller_introspect.json
 */
internal fun SetupActivity.writeControllerIntrospectJson(game: String? = null) {
    try {
        val cfg = File(filesDir, "controller_config.json")
        if (!cfg.exists()) {
            Log.w("DXX-Setup", "No controller_config.json to introspect")
            return
        }
        val json = JSONObject(cfg.readText())
        val gameId = game ?: "d2"
        val meta = if (gameId == "d1") KC_JOY_META_D1 else KC_JOY_META_D2
        val joyArr =
            json.optJSONArray("key_settings_joystick_$gameId")
                ?: json.optJSONArray("key_settings_joystick")
        val ct = json.optInt("control_type", 1)

        val n = meta.size
        val items = JSONArray()
        var boundCount = 0
        var boundControls = 0
        val gyroFallback = mapOf(19 to 7, 21 to 6)
        for (i in 0 until n) {
            var value = if (joyArr != null && i < joyArr.length()) joyArr.getInt(i) else 255
            if (value == 255 && i in gyroFallback) value = gyroFallback[i]!!
            if (meta[i].type == "invert") {
                value = if (value == 1) 1 else 0
            }
            val bound = value != 255
            if (bound) boundCount++
            if (bound && meta[i].type != "invert") boundControls++
            val item = JSONObject()
            item.put("index", i)
            item.put("name", meta[i].name)
            item.put("type", meta[i].type)
            item.put("value", value)
            item.put("bound", bound)
            items.put(item)
        }

        val root = JSONObject()
        root.put("source", "launcher")
        root.put("game", gameId)
        val jc = JSONObject()
        jc.put("control_type", ct)
        jc.put("bound_count", boundCount)
        jc.put("bound_controls", boundControls)
        jc.put("total_count", n)
        jc.put("items", items)
        root.put("joystick_controls", jc)
        root.put("keyboard_settings", json.optJSONArray("key_settings_keyboard") ?: JSONArray())

        if (json.has("bindings")) root.put("bindings", json.getJSONObject("bindings"))
        if (json.has("inverts")) root.put("inverts", json.getJSONArray("inverts"))

        val outFile = File(filesDir, "controller_introspect.json")
        AtomicFilePublication.writeUtf8(outFile, root.toString(2))
        Log.i("DXX-Setup", "Controller introspect written: ${outFile.absolutePath}")
    } catch (e: Exception) {
        Log.e("DXX-Setup", "Failed to write controller introspect JSON", e)
    }
}

internal fun SetupActivity.writeMpIntrospectJson() {
    try {
        val s = com.dxxredux.app.multiplayer.MatchmakingStateHolder.state.value
        val root = JSONObject()
        root.put("status", s.status.name)
        root.put("callsign", s.callsign)
        root.put("player_id", s.playerId ?: JSONObject.NULL)
        root.put("nav", s.nav.name)
        root.put("error", s.errorMessage ?: JSONObject.NULL)

        val lobby = s.currentLobby
        if (lobby != null) {
            val lj = JSONObject()
            lj.put("lobby_id", lobby.lobbyId)
            lj.put("is_host", lobby.isHost)
            lj.put("player_count", lobby.players.size)
            val pArr = JSONArray()
            for (p in lobby.players) {
                val pj = JSONObject()
                pj.put("player_id", p.playerId)
                pj.put("callsign", p.callsign)
                pj.put("ready", p.ready)
                pArr.put(pj)
            }
            lj.put("players", pArr)
            root.put("lobby", lj)
        }

        root.put("lobby_count", s.lobbies.size)
        val lobbiesArr = JSONArray()
        for (l in s.lobbies) {
            val lj = JSONObject()
            lj.put("lobby_id", l.lobbyId)
            lj.put("host_callsign", l.hostCallsign)
            lj.put("game", l.game)
            lj.put("mission", l.gameInfo["mission"]?.jsonPrimitive?.content ?: "")
            lj.put("mode", l.gameInfo["mode"]?.jsonPrimitive?.content ?: "")
            lj.put("player_count", l.playerCount)
            lj.put("joinable", l.joinable)
            lobbiesArr.put(lj)
        }
        root.put("lobbies", lobbiesArr)

        val chatArr = JSONArray()
        for (msg in s.chatMessages) {
            val mj = JSONObject()
            mj.put("from", msg.fromCallsign)
            mj.put("text", msg.text)
            mj.put("is_me", msg.isMe)
            chatArr.put(mj)
        }
        root.put("chat", chatArr)

        root.put("game_launch_pending", s.gameLaunchInfo != null)

        val logArr = JSONArray()
        for (line in s.statusLog.takeLast(20)) {
            logArr.put(line)
        }
        root.put("log", logArr)

        val file = File(filesDir, "mp_introspect.json")
        file.writeText(root.toString(2))
        Log.i("DXX-MP", "MP introspection written to ${file.absolutePath}")
    } catch (e: Exception) {
        Log.e("DXX-MP", "Failed to write MP introspection", e)
    }
}

internal fun SetupActivity.writeIntrospectJson(buttons: List<SetupActivity.ButtonInfo>) {
    try {
        val dir = filesDir
        val fsm = FileSetManager(dir)
        val activeSet = fsm.getActive()
        val setDir = fsm.getSetDir(activeSet)
        val manifest = AssetManifest(setDir)
        val safManifest = fsm.safManifestForSet(activeSet)
        val d2FileList = detectD2FileList(setDir, safManifest)
        val d2Statuses = checkFiles(setDir, d2FileList, manifest, safManifest)
        val d1Statuses = checkFiles(setDir, D1_FILES, manifest, safManifest)
        val d2Ready = launchDataReadyForGame("d2", setDir, manifest, safManifest)
        val d1Ready = launchDataReadyForGame("d1", setDir, manifest, safManifest)
        val d1InD2 = d1InD2Readiness(dir, setDir, manifest, safManifest)

        val root = JSONObject()
        root.put("screen", "setup")
        root.put("can_launch", d2Ready || d1Ready)
        root.put("active_set", activeSet)
        val runningGamePid = automationRunningGameProcessPid()
        val hasReturnableGameActivity = automationHasReturnableGameActivity()
        root.put("game_running", hasReturnableGameActivity || runningGamePid != null)
        root.put("has_returnable_game_activity", hasReturnableGameActivity)
        root.put("running_game_pid", runningGamePid ?: -1)

        val allFiles = dir.listFiles()?.map { it.name }?.sorted() ?: emptyList()
        root.put("files_on_disk", JSONArray(allFiles))

        val setFiles = setDir.listFiles()?.map { it.name }?.sorted() ?: emptyList()
        root.put("set_files", JSONArray(setFiles))
        val recursiveSetFiles =
            setDir
                .walkTopDown()
                .filter { it.isFile }
                .map { it.relativeTo(setDir).invariantSeparatorsPath }
                .sorted()
                .toList()
        root.put("set_files_recursive", JSONArray(recursiveSetFiles))
        root.put("active_set_path", setDir.absolutePath)
        val importState = SetupImportTracker.snapshot()
        root.put(
            "import_state",
            JSONObject()
                .put("kind", importState.kind)
                .put("status", importState.status)
                .put("result_count", importState.resultCount)
                .put("error", importState.error),
        )

        val stagedInputDemos = InputDemoManager.listStagedDemos(dir)
        root.put("staged_input_demo_count", stagedInputDemos.size)
        root.put("staged_input_demos", inputDemoArray(stagedInputDemos))

        val installedInputDemos = InputDemoManager.listInstalledDemos(setDir)
        root.put("installed_input_demo_count", installedInputDemos.size)
        root.put("installed_input_demos", inputDemoArray(installedInputDemos))

        val prefs = getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
        val networkLogEnabled = prefs.getBoolean(DebugLogCategory.prefKey(DebugLogCategory.NETWORK), false)
        val graphicsLogEnabled = prefs.getBoolean(DebugLogCategory.prefKey(DebugLogCategory.GRAPHICS), false)
        val textureLogEnabled = prefs.getBoolean(DebugLogCategory.prefKey(DebugLogCategory.TEXTURE), false)
        val gameLogEnabled = prefs.getBoolean(DebugLogCategory.prefKey(DebugLogCategory.GAME), false)
        val launcherLogEnabled = prefs.getBoolean(DebugLogCategory.prefKey(DebugLogCategory.LAUNCHER), false)
        val profilingLogEnabled = prefs.getBoolean(DebugLogCategory.prefKey(DebugLogCategory.PROFILING), false)
        val coopDesyncLogEnabled = prefs.getBoolean(DebugLogCategory.prefKey(DebugLogCategory.COOP_DESYNC), false)
        val debugPrefs = JSONObject()
        debugPrefs.put(
            "show_video_info_debug_options",
            prefs.getBoolean(PREF_SHOW_VIDEO_INFO_DEBUG_OPTIONS, false),
        )
        debugPrefs.put("network_log_enabled", networkLogEnabled)
        debugPrefs.put("graphics_log_enabled", graphicsLogEnabled)
        debugPrefs.put("texture_log_enabled", textureLogEnabled)
        debugPrefs.put("game_log_enabled", gameLogEnabled)
        debugPrefs.put("launcher_log_enabled", launcherLogEnabled)
        debugPrefs.put("profiling_log_enabled", profilingLogEnabled)
        debugPrefs.put("coop_desync_log_enabled", coopDesyncLogEnabled)
        debugPrefs.put("graphics_debug_logging", graphicsLogEnabled && textureLogEnabled)
        root.put("debug_prefs", debugPrefs)

        val newestResumeCandidate = ResumeSaveBridge.findNewest(dir)
        val resumeOfferEnabled =
            prefs.getBoolean(PREF_SHOW_RESUME_OFFER, true) &&
                when (newestResumeCandidate?.game) {
                    "d1" -> d1Ready
                    "d2", null -> d2Ready
                    else -> false
                } &&
                newestResumeCandidate != null
        root.put("resume_offer_enabled", resumeOfferEnabled)
        val demoInstallerOfferEnabled = prefs.getBoolean(PREF_SHOW_DEMO_INSTALLER_OFFER, true)
        root.put("demo_installer_offer_enabled", demoInstallerOfferEnabled)
        root.put(
            "demo_installer_offers",
            JSONArray(
                visibleDemoInstallerOffers(
                    showDemoInstallerOffer = demoInstallerOfferEnabled,
                    d1Ready = d1Ready,
                    d2Ready = d2Ready,
                ).map { it.game },
            ),
        )

        val d2 = JSONObject()
        d2.put("ready", d2Ready)
        d2.put("files", fileStatusArray(d2Statuses))
        root.put("d2", d2)

        val d1 = JSONObject()
        d1.put("ready", d1Ready)
        d1.put("files", fileStatusArray(d1Statuses))
        root.put("d1", d1)

        val d1InD2Json = JSONObject()
        d1InD2Json.put("needed", d1InD2.needed)
        d1InD2Json.put("ready", d1InD2.ready)
        d1InD2Json.put("degraded", d1InD2.degraded)
        d1InD2Json.put("blocked", d1InD2.blocked)
        d1InD2Json.put("d2_ready", d1InD2.d2Ready)
        d1InD2Json.put("d1_assets_ready", d1InD2.d1AssetsReady)
        d1InD2Json.put("files", fileStatusArray(d1InD2.d1AssetStatuses))
        root.put("d1_in_d2", d1InD2Json)

        newestResumeCandidate?.let { root.put("resume_candidate", it.toJson()) }

        if (downloadStates.isNotEmpty()) {
            val dl = JSONObject()
            for ((name, progress) in downloadStates) {
                dl.put(
                    name,
                    when (progress) {
                        -2 -> "complete"
                        -1 -> "error"
                        else -> "$progress%"
                    },
                )
            }
            root.put("downloads", dl)
        }

        val setsArr = JSONArray()
        for (setInfo in fsm.listSets()) {
            val so = JSONObject()
            so.put("name", setInfo.name)
            val sd = fsm.getSetDir(setInfo.name)
            so.put("file_count", sd.listFiles()?.count { it.isFile } ?: 0)
            so.put("active", setInfo.name == activeSet)
            setsArr.put(so)
        }
        root.put("sets", setsArr)

        val srcManager = AudioSourceManager(dir)
        val sources = srcManager.getSources()
        if (sources.isNotEmpty()) {
            val audioArr = JSONArray()
            for (src in sources) {
                val ao = JSONObject()
                ao.put("id", src.id)
                ao.put("label", src.discLabel)
                ao.put("disc_id", src.discId)
                ao.put("cue_path", src.cuePath)
                ao.put("track_count", src.trackCount)
                ao.put("audio_track_count", src.audioTrackCount)
                if (src.trackNames.isNotEmpty()) {
                    val tn = JSONObject()
                    for ((k, v) in src.trackNames) tn.put(k.toString(), v)
                    ao.put("track_names", tn)
                }
                audioArr.put(ao)
            }
            root.put("audio_sources", audioArr)
        }
        if (findGogPair(setDir) != null) root.put("has_legacy_gog_audio", true)

        val musicPreview = JSONObject()
        val midiState = MidiPreviewBridge.getState()
        val midiObj = JSONObject()
        midiObj.put(
            "state",
            when (midiState.state) {
                MidiPreviewBridge.STATE_PLAYING -> "playing"
                MidiPreviewBridge.STATE_PAUSED -> "paused"
                else -> "stopped"
            },
        )
        midiObj.put("position_ms", midiState.positionMs)
        midiObj.put("duration_ms", midiState.durationMs)
        musicPreview.put("midi", midiObj)
        val cdState = CdPreviewBridge.getState()
        val cdObj = JSONObject()
        cdObj.put(
            "state",
            when (cdState.state) {
                CdPreviewBridge.STATE_PLAYING -> "playing"
                CdPreviewBridge.STATE_PAUSED -> "paused"
                else -> "stopped"
            },
        )
        cdObj.put("position_ms", cdState.positionMs)
        cdObj.put("duration_ms", cdState.durationMs)
        musicPreview.put("cd", cdObj)
        val midiEnum = MidiEnumerationBridge.enumerateTracks(setDir.absolutePath)
        if (midiEnum.sources.isNotEmpty()) {
            val midiSrcArr = JSONArray()
            for (ms in midiEnum.sources) {
                val mso = JSONObject()
                mso.put("id", ms.id)
                mso.put("label", ms.label)
                mso.put("track_count", ms.tracks.size)
                midiSrcArr.put(mso)
            }
            musicPreview.put("midi_sources", midiSrcArr)
        }
        root.put("music_preview", musicPreview)

        val buttonsArr = JSONArray()
        for (btn in buttons) {
            val bo = JSONObject()
            bo.put("text", btn.text)
            bo.put("enabled", btn.enabled)
            bo.put("focused", btn.focused)
            bo.put("x", btn.centerX.toInt())
            bo.put("y", btn.centerY.toInt())
            bo.put("w", btn.width.toInt())
            bo.put("h", btn.height.toInt())
            buttonsArr.put(bo)
        }
        root.put("buttons", buttonsArr)

        val outFile = File(dir, "setup_introspect.json")
        AtomicFilePublication.writeUtf8(outFile, root.toString())
        Log.i("DXX-Setup", "Introspect written: ${outFile.absolutePath}")
    } catch (e: Exception) {
        Log.e("DXX-Setup", "Failed to write introspect JSON", e)
    }
}

private fun inputDemoArray(demos: List<StagedInputDemo>): JSONArray =
    JSONArray(
        demos.map { demo ->
            JSONObject()
                .put("filename", demo.file.name)
                .put("game", demo.game)
                .put("mission", demo.mission)
                .put("level", demo.level)
                .put("frame_count", demo.frameCount)
                .put("has_rng_trace", demo.traceFile != null)
                .put("has_classic_demo", demo.classicDemoFile != null)
                .put("header_readable", demo.headerReadable)
        },
    )

private fun fileStatusArray(statuses: List<FileStatus>): JSONArray {
    val arr = JSONArray()
    for (s in statuses) {
        val obj = JSONObject()
        obj.put("filename", s.info.filename)
        obj.put("required", s.info.required)
        obj.put("found", s.found)
        if (s.foundName != null) obj.put("found_as", s.foundName)
        if (s.info.alternatives.isNotEmpty()) {
            obj.put("alternatives", JSONArray(s.info.alternatives))
        }
        if (s.info.downloadUrl != null) {
            obj.put("download_url", s.info.downloadUrl)
        }
        obj.put("description", s.info.description)
        if (s.safUri != null) {
            obj.put("saf_linked", true)
            obj.put("saf_uri", s.safUri)
        }
        if (s.manifestEntry != null) {
            obj.put("sha256", s.manifestEntry.sha256)
            obj.put("version", s.manifestEntry.versionDisplay)
            if (s.manifestEntry.isExternal) {
                obj.put("source_uri", s.manifestEntry.sourceUri)
                obj.put("external", true)
            }
            if (!s.found) {
                obj.put("missing_from_disk", true)
            }
        }
        arr.put(obj)
    }
    return arr
}

internal fun SetupActivity.clearSaveFilesForAutomation(): Int {
    val saveRegex = Regex("""\.(?:sg|mg)[0-9]$""", RegexOption.IGNORE_CASE)
    var deleted = 0
    clearPendingResumeLaunchState(this)
    for (subdir in arrayOf("d1x-redux", "d2x-redux")) {
        val dir = File(filesDir, subdir)
        if (!dir.exists()) continue
        dir
            .walkTopDown()
            .filter { it.isFile && saveRegex.containsMatchIn(it.name) }
            .forEach {
                if (it.delete()) deleted++
            }
    }
    return deleted
}

internal fun SetupActivity.clearPilotFilesForAutomation(): Int {
    val pilotRegex = Regex("""\.(?:plr|plx)$""", RegexOption.IGNORE_CASE)
    var deleted = 0
    for (subdir in arrayOf("d1x-redux", "d2x-redux")) {
        val dir = File(filesDir, subdir)
        if (!dir.exists()) continue
        dir
            .walkTopDown()
            .filter { it.isFile && pilotRegex.containsMatchIn(it.name) }
            .forEach {
                if (it.delete()) deleted++
            }
    }
    return deleted
}
