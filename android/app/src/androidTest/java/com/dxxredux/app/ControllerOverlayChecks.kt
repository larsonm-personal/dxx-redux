package com.dxxredux.app

import android.app.Instrumentation
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.RectF
import android.os.SystemClock
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import java.io.File

/** Exercises the production overlay with real touch events, controller navigation and the input mixer */
internal class ControllerOverlayChecks(
    private val instrumentation: Instrumentation,
) {
    private fun <T> onMain(block: () -> T): T {
        var result: Result<T>? = null
        instrumentation.runOnMainSync { result = runCatching(block) }
        return result!!.getOrThrow()
    }

    private fun field(
        view: TouchOverlayView,
        name: String,
    ): Any? =
        TouchOverlayView::class.java
            .getDeclaredField(name)
            .apply { isAccessible = true }
            .get(view)

    @Suppress("UNCHECKED_CAST")
    private fun rects(
        view: TouchOverlayView,
        name: String,
    ): List<RectF> = field(view, name) as List<RectF>

    @Suppress("UNCHECKED_CAST")
    private fun adminActions(view: TouchOverlayView): List<Int> =
        TouchOverlayView::class.java
            .getDeclaredMethod("currentAdminTrayActions")
            .apply { isAccessible = true }
            .invoke(view) as List<Int>

    private fun render(view: TouchOverlayView) {
        val bitmap = Bitmap.createBitmap(1000, 600, Bitmap.Config.ARGB_8888)
        try {
            view.draw(Canvas(bitmap))
        } finally {
            bitmap.recycle()
        }
    }

    private fun touch(
        view: TouchOverlayView,
        action: Int,
        x: Float,
        y: Float,
    ) {
        val now = SystemClock.uptimeMillis()
        val event = MotionEvent.obtain(now, now, action, x, y, 0)
        try {
            check(view.dispatchTouchEvent(event)) { "Overlay did not consume touch action $action" }
        } finally {
            event.recycle()
        }
    }

    private fun tap(
        view: TouchOverlayView,
        rect: RectF,
    ) {
        touch(view, MotionEvent.ACTION_DOWN, rect.centerX(), rect.centerY())
        touch(view, MotionEvent.ACTION_UP, rect.centerX(), rect.centerY())
    }

    private fun key(
        view: TouchOverlayView,
        code: Int,
    ) {
        check(view.handleControllerMenuKey(code, KeyEvent.ACTION_DOWN))
        // Screen transitions may have closed the menu before key-up
        view.handleControllerMenuKey(code, KeyEvent.ACTION_UP)
    }

    fun run() {
        val context = instrumentation.targetContext
        val files = listOf("touch_layout.json", "touch_layout_slots.json").map { File(context.filesDir, it) }
        val snapshots = files.associateWith { if (it.exists()) it.readBytes() else null }
        val controllerLayout =
            TouchLayoutRepository
                .loadBundledPresets(
                    context,
                ).first { isControllerMenuOnlyTouchLayout(it) }
        val saved =
            TouchLayout(
                name = "Controller test saved touch layout",
                buttons =
                    listOf(
                        ButtonControl(id = "bomb", xPct = 50f, yPct = 50f, binding = TouchBindings.BTN_DROP_BOMB),
                    ),
                radialMenus =
                    listOf("PriWpn", "SecWpn", "Guide").map {
                        RadialMenuControl(id = it, xPct = 10f, yPct = 10f, segments = emptyList())
                    },
                gyro = GyroConfig(enabled = true),
            )
        for (game in listOf("d1", "d2")) {
            val edges = mutableListOf<Pair<Int, Int>>()
            val admin = mutableListOf<Int>()
            var mapOpened = false
            val view =
                onMain {
                    TouchOverlayView(context).apply {
                        gameVariant = game
                        touchControlsEnabled = false
                        remainingActionsLayoutProvider = { saved }
                        workingControllerInUseProvider = { true }
                        controllerBoundActionBindingsProvider = { setOf(TouchBindings.BTN_FIRE_FLARE) }
                        inputMixer = InputMixer({ binding, pressed -> edges.add(binding to pressed) }, { _, _, _ -> })
                        adminTrayCallback = { admin.add(it) }
                        mapButtonCallback = { mapOpened = true }
                        setLayout(effectiveTouchOverlayLayout(saved, controllerLayout, false, true))
                        isActive = true
                        measure(
                            View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                            View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.EXACTLY),
                        )
                        layout(0, 0, 1000, 600)
                    }
                }
            val actions =
                remainingKeyTouchActions(
                    saved,
                    game,
                    touchControlsEnabled = false,
                    workingControllerInUse = true,
                    controllerBoundBindings = setOf(TouchBindings.BTN_FIRE_FLARE),
                )
            val bombIndex = actions.indexOfFirst { it.binding == TouchBindings.BTN_DROP_BOMB }
            check(bombIndex >= 0)
            onMain {
                check(view.getLayout().name == CONTROLLER_MENU_TOUCH_PRESET_NAME)
                check(view.getLayout().gyro == saved.gyro)
                view.cycleControllerMenu()
                render(view)
                check(rects(view, "remainingActionItemRects").size == actions.size)
                tap(view, RectF(1f, 1f, 9f, 9f))
                check(view.isControllerMenuOpen()) { "Outside tap dismissed controller extra actions" }
                tap(view, rects(view, "remainingActionItemRects")[bombIndex])
                check(view.isControllerMenuOpen()) { "Action tap dismissed controller extra actions" }
            }
            Thread.sleep(100)
            onMain {
                key(view, KeyEvent.KEYCODE_BUTTON_A)
                check(view.isControllerMenuOpen()) { "Controller A dismissed repeatable action" }
            }
            Thread.sleep(100)
            onMain {
                check(edges.count { it == (TouchBindings.BTN_DROP_BOMB to 1) } == 2)
                check(edges.count { it == (TouchBindings.BTN_DROP_BOMB to 0) } == 2)
                if (game == "d2") {
                    val heldIndex = actions.indexOfFirst { it.binding == TouchBindings.BTN_ENERGY_SHIELD }
                    val rect = rects(view, "remainingActionItemRects")[heldIndex]
                    touch(view, MotionEvent.ACTION_DOWN, rect.centerX(), rect.centerY())
                    check(edges.last() == (TouchBindings.BTN_ENERGY_SHIELD to 1))
                    touch(view, MotionEvent.ACTION_UP, rect.centerX(), rect.centerY())
                    check(edges.last() == (TouchBindings.BTN_ENERGY_SHIELD to 0))
                    check(view.isControllerMenuOpen())
                    check(view.handleControllerMenuKey(KeyEvent.KEYCODE_BUTTON_A, KeyEvent.ACTION_DOWN))
                    check(edges.last() == (TouchBindings.BTN_ENERGY_SHIELD to 1))
                    check(view.handleControllerMenuKey(KeyEvent.KEYCODE_BUTTON_A, KeyEvent.ACTION_UP))
                    check(edges.last() == (TouchBindings.BTN_ENERGY_SHIELD to 0))
                    touch(view, MotionEvent.ACTION_DOWN, rect.centerX(), rect.centerY())
                    touch(view, MotionEvent.ACTION_CANCEL, rect.centerX(), rect.centerY())
                    check(edges.last() == (TouchBindings.BTN_ENERGY_SHIELD to 0))
                    check(view.isControllerMenuOpen()) { "Canceled gesture dismissed controller menu" }
                }
                key(view, KeyEvent.KEYCODE_BUTTON_B)
                check(!view.isControllerMenuOpen())
                view.cycleControllerMenu()
                view.cycleControllerMenu()
            }
            Thread.sleep(300)
            onMain {
                check(view.isAdminTrayOpen())
                render(view)
                tap(view, RectF(490f, 10f, 510f, 30f))
                check(view.isAdminTrayOpen()) { "Outside tap dismissed controller settings" }
                val autoLevelIndex = adminActions(view).indexOf(TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL)
                tap(view, rects(view, "adminTrayRects")[autoLevelIndex])
                check(
                    admin.last() == TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL,
                ) { "Wrong settings action: ${admin.last()}" }
                check(view.isAdminTrayOpen())
                repeat(autoLevelIndex / 3) { key(view, KeyEvent.KEYCODE_DPAD_DOWN) }
                repeat(autoLevelIndex % 3) { key(view, KeyEvent.KEYCODE_DPAD_RIGHT) }
                check(view.controllerNavigationState()["admin_index"] == autoLevelIndex)
                repeat(2) { key(view, KeyEvent.KEYCODE_BUTTON_A) }
                check(admin.size == 3 && admin.all { it == TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL }) {
                    "Expected three auto-level activations, got $admin"
                }
                check(view.isAdminTrayOpen()) { "Controller activation dismissed settings" }
                key(view, KeyEvent.KEYCODE_BUTTON_B)
            }
            Thread.sleep(300)
            onMain {
                check(!view.isControllerMenuOpen()) { "Controller B did not dismiss settings" }
                view.cycleControllerMenu()
                view.cycleControllerMenu()
            }
            Thread.sleep(300)
            onMain {
                view.cycleControllerMenu()
            }
            Thread.sleep(300)
            onMain {
                check(!view.isControllerMenuOpen()) { "Menu did not dismiss settings" }
                view.cycleControllerMenu()
                render(view)
                tap(
                    view,
                    rects(view, "remainingActionItemRects")[
                        actions.indexOfFirst {
                            it.binding ==
                                TouchBindings.BTN_AUTOMAP
                        },
                    ],
                )
                check(!view.isControllerMenuOpen()) { "Automap transition retained extra actions" }
            }
            Thread.sleep(100)
            onMain {
                check(mapOpened)
                view.touchControlsEnabled = true
                view.setLayout(effectiveTouchOverlayLayout(saved, controllerLayout, true, true))
                check(view.getLayout() == saved)
                render(view)
                tap(view, field(view, "remainingActionButtonRect") as RectF)
                check(view.isControllerMenuOpen())
                tap(view, RectF(1f, 1f, 9f, 9f))
                check(!view.isControllerMenuOpen()) { "Touch-opened menu no longer dismisses on outside tap" }
                render(view)
                tap(view, field(view, "remainingActionButtonRect") as RectF)
                render(view)
                tap(view, rects(view, "remainingActionItemRects")[0])
                check(!view.isControllerMenuOpen()) { "Touch-opened menu no longer dismisses after action" }
                view.isActive = false
            }
            Thread.sleep(100)
        }
        for ((file, bytes) in snapshots) {
            check(
                if (bytes ==
                    null
                ) {
                    !file.exists()
                } else {
                    file.readBytes().contentEquals(bytes)
                },
            ) { "Saved layout changed: ${file.name}" }
        }
        checkControllerAwareTouch()
    }

    private fun checkControllerAwareTouch() {
        val context = instrumentation.targetContext
        val presets = TouchLayoutRepository.loadBundledPresets(context)
        val full = presets.first { it.name == DEFAULT_TOUCH_PRESET_NAME }
        val menus = presets.first { isControllerMenuOnlyTouchLayout(it) }
        val bindings = loadDefaultBindings(context)
        val available =
            ControllerInputAvailability(
                connected = true,
                buttons = (0..31).toSet(),
                axes = AXIS_CONTROLS.values.associateWith { setOf(false, true) },
            )
        for (game in listOf("d1", "d2")) {
            val covered = controllerTouchCoverage(bindings, emptySet(), available, game)
            val filtered = effectiveTouchOverlayLayout(full, menus, true, true, covered, game)
            check(filtered.sticks.isEmpty()) { "$game: default sticks not covered" }
            check(
                filtered.buttons.map { it.binding }.toSet() ==
                    setOf(
                        TouchBindings.BTN_AUTOMAP,
                        TouchBindings.META_REWIND,
                        TouchBindings.META_QUICK_SAVE,
                        TouchBindings.META_QUICK_LOAD,
                    ),
            ) { "$game: unexpected default buttons ${filtered.buttons}" }
            check(filtered.radialMenus == full.radialMenus && filtered.diagnostics == full.diagnostics)
            check(filtered.moreActions == full.moreActions)
            check(effectiveTouchOverlayLayout(full, menus, true, false, ControllerTouchCoverage(), game) == full)

            val edges = mutableListOf<Pair<Int, Int>>()
            val axes = mutableMapOf<Int, Float>()
            val meta = mutableListOf<Pair<Int, Boolean>>()
            val saved =
                TouchLayout(
                    sticks =
                        listOf(
                            AnalogStickControl(
                                "move",
                                20f,
                                50f,
                                axisX = TouchBindings.AXIS_LEFT_X,
                                axisY = TouchBindings.AXIS_LEFT_Y,
                            ),
                        ),
                    buttons = listOf(ButtonControl("fire", 75f, 50f, binding = TouchBindings.BTN_FIRE_PRIMARY)),
                    radialMenus =
                        listOf(
                            RadialMenuControl(
                                "Guide",
                                45f,
                                25f,
                                segments =
                                    listOf(
                                        RadialSegment(
                                            "Energy",
                                            TouchBindings.META_GUIDE_FIND_ENERGY,
                                            bindingType = "action",
                                        ),
                                    ),
                                centerBinding = TouchBindings.META_GUIDE_FIND_ENERGY,
                            ),
                        ),
                )
            var currentCoverage = ControllerTouchCoverage()
            onMain {
                val view =
                    TouchOverlayView(context).apply {
                        gameVariant = game
                        inputMixer =
                            InputMixer(
                                { binding, pressed -> edges.add(binding to pressed) },
                                { axis, value, _ -> axes[axis] = value },
                            )
                        axisCallback = { axis, value -> inputMixer?.setAxis(axis, "touch", value) }
                        metaActionCallback = { binding, pressed -> meta.add(binding to pressed) }
                        controllerBoundActionBindingsProvider = { currentCoverage.actions }
                        workingControllerInUseProvider = { currentCoverage.connected }
                        setLayout(saved)
                        isActive = true
                        measure(
                            View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                            View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.EXACTLY),
                        )
                        layout(0, 0, 1000, 600)
                    }

                fun connect(coverage: ControllerTouchCoverage) {
                    currentCoverage = coverage
                    view.setLayout(controllerFilteredTouchLayout(saved, coverage, game), preserveMenus = true)
                    render(view)
                }
                render(view)
                touch(view, MotionEvent.ACTION_DOWN, 750f, 300f)
                check(edges.last() == (TouchBindings.BTN_FIRE_PRIMARY to 1))
                connect(covered)
                check(edges.last() == (TouchBindings.BTN_FIRE_PRIMARY to 0)) { "Connect left touch fire held" }
                touch(view, MotionEvent.ACTION_UP, 750f, 300f)
                val count = edges.size
                tap(view, RectF(745f, 295f, 755f, 305f))
                check(edges.size == count) { "Hidden button captured touch" }
                connect(ControllerTouchCoverage())
                touch(view, MotionEvent.ACTION_DOWN, 220f, 300f)
                check(axes.values.any { it != 0f }) { "Restored stick did not respond" }
                connect(covered)
                check(axes.values.all { it == 0f }) { "Connect left touch axis held" }
                touch(view, MotionEvent.ACTION_UP, 220f, 300f)
                view.setLayout(saved.copy(buttons = saved.buttons.map { it.copy(toggle = true) }))
                tap(view, RectF(745f, 295f, 755f, 305f))
                check(edges.last() == (TouchBindings.BTN_FIRE_PRIMARY to 1))
                connect(covered)
                check(edges.last() == (TouchBindings.BTN_FIRE_PRIMARY to 0)) { "Hidden toggle remained latched" }
                view.setLayout(
                    saved.copy(
                        sticks =
                            saved.sticks.map {
                                it.copy(
                                    doubleTapBinding = TouchBindings.BTN_FIRE_PRIMARY,
                                    doubleTapMode = DoubleTapMode.LATCH_SINGLE,
                                )
                            },
                    ),
                )
                repeat(2) { tap(view, RectF(195f, 295f, 205f, 305f)) }
                check(edges.last() == (TouchBindings.BTN_FIRE_PRIMARY to 1))
                connect(covered)
                check(edges.last() == (TouchBindings.BTN_FIRE_PRIMARY to 0)) { "Hidden double-tap remained latched" }
                if (game == "d2") {
                    touch(view, MotionEvent.ACTION_DOWN, 450f, 150f)
                    val radial = (field(view, "radialStates") as List<*>).single()!!
                    val open = radial.javaClass.getDeclaredField("isOpen").apply { isAccessible = true }
                    check(open.getBoolean(radial)) { "Guide menu did not open by touch" }
                    connect(ControllerTouchCoverage())
                    check((field(view, "radialStates") as List<*>).single() === radial && open.getBoolean(radial)) {
                        "Disconnect closed Guide menu"
                    }
                    touch(view, MotionEvent.ACTION_MOVE, 450f, 150f)
                    touch(view, MotionEvent.ACTION_UP, 450f, 150f)
                    check(
                        meta.any { it == (TouchBindings.META_GUIDE_FIND_ENERGY to true) },
                    ) { "Guide action did not fire" }
                }
                connect(covered)
                view.cycleControllerMenu()
                check(view.isControllerMenuOpen())
                connect(ControllerTouchCoverage())
                check(view.isControllerMenuOpen()) { "Disconnect closed More menu" }
                key(view, KeyEvent.KEYCODE_BUTTON_B)
                tap(view, RectF(745f, 295f, 755f, 305f))
                check(
                    edges.takeLast(2) ==
                        listOf(TouchBindings.BTN_FIRE_PRIMARY to 1, TouchBindings.BTN_FIRE_PRIMARY to 0),
                )
                view.isActive = false
            }
        }
    }
}
