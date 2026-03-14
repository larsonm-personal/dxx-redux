package com.dxxredux.app

import android.util.Log
import org.json.JSONArray
import org.json.JSONObject

/**
 * Translates between the internal integer-based config format (used at runtime
 * in touch_layout.json / controller_config.json) and a human-readable format
 * (used in bundled asset configs and export/import files).
 *
 * In the human-readable format, binding and axis fields use string names
 * ("Fire Primary", "Left Stick X") instead of raw integers (0, 1).
 */
object HumanReadableConfig {
    private const val TAG = "HumanReadableConfig"

    /** Result of a parse operation: the parsed value (if possible) plus any warnings. */
    data class ParseResult<T>(
        val value: T?,
        val warnings: List<String>,
    ) {
        val success: Boolean get() = value != null
    }

    // ── Touch Layout: internal -> human-readable ──

    fun touchLayoutToHumanJson(layout: TouchLayout): JSONObject {
        val j = JSONObject()
        j.put("type", "touch_layout")
        j.put("version", layout.version)
        j.put("name", layout.name)
        j.put("globalOpacity", layout.globalOpacity.toDouble())
        j.put("sticks", JSONArray(layout.sticks.map { stickToHuman(it) }))
        j.put("buttons", JSONArray(layout.buttons.map { buttonToHuman(it) }))
        j.put("sliders", JSONArray(layout.sliders.map { sliderToHuman(it) }))
        j.put("radialMenus", JSONArray(layout.radialMenus.map { radialToHuman(it) }))
        j.put("dpads", JSONArray(layout.dpads.map { dpadToHuman(it) }))
        j.put("gyro", gyroToHuman(layout.gyro))
        return j
    }

    private fun stickToHuman(s: AnalogStickControl): JSONObject {
        val j = s.toJson()
        j.put("axisX", TouchBindings.axisToName(s.axisX))
        j.put("axisY", TouchBindings.axisToName(s.axisY))
        if (s.buttonMode) {
            j.put("negXBinding", TouchBindings.bindingToName(s.negXBinding))
            j.put("posXBinding", TouchBindings.bindingToName(s.posXBinding))
            j.put("negYBinding", TouchBindings.bindingToName(s.negYBinding))
            j.put("posYBinding", TouchBindings.bindingToName(s.posYBinding))
        }
        return j
    }

    private fun buttonToHuman(b: ButtonControl): JSONObject {
        val j = b.toJson()
        j.put("binding", TouchBindings.bindingToName(b.binding))
        return j
    }

    private fun sliderToHuman(s: SliderControl): JSONObject {
        val j = s.toJson()
        j.put("axis", TouchBindings.axisToName(s.axis))
        return j
    }

    private fun radialToHuman(r: RadialMenuControl): JSONObject {
        val j = r.toJson()
        val segs = JSONArray()
        for (seg in r.segments) {
            val sj = seg.toJson()
            sj.put("binding", TouchBindings.bindingToName(seg.binding))
            segs.put(sj)
        }
        j.put("segments", segs)
        if (r.centerBinding >= 0) {
            j.put("centerBinding", TouchBindings.bindingToName(r.centerBinding))
        }
        return j
    }

    private fun dpadToHuman(d: DPadControl): JSONObject {
        val j = d.toJson()
        j.put("up", TouchBindings.bindingToName(d.upBinding))
        j.put("down", TouchBindings.bindingToName(d.downBinding))
        j.put("left", TouchBindings.bindingToName(d.leftBinding))
        j.put("right", TouchBindings.bindingToName(d.rightBinding))
        return j
    }

    private fun gyroToHuman(g: GyroConfig): JSONObject {
        val j = g.toJson()
        j.put("axisX", TouchBindings.axisToName(g.axisX))
        j.put("axisY", TouchBindings.axisToName(g.axisY))
        return j
    }

    // ── Touch Layout: human-readable -> internal ──

    fun humanJsonToTouchLayout(json: JSONObject): ParseResult<TouchLayout> {
        val warnings = mutableListOf<String>()
        try {
            val sticks =
                parseArraySafe(json, "sticks", warnings) { j, w ->
                    parseStick(j, w)
                }
            val buttons =
                parseArraySafe(json, "buttons", warnings) { j, w ->
                    parseButton(j, w)
                }
            val sliders =
                parseArraySafe(json, "sliders", warnings) { j, w ->
                    parseSlider(j, w)
                }
            val radials =
                parseArraySafe(json, "radialMenus", warnings) { j, w ->
                    parseRadial(j, w)
                }
            val dpads =
                parseArraySafe(json, "dpads", warnings) { j, w ->
                    parseDPad(j, w)
                }
            val gyro =
                if (json.has("gyro")) {
                    parseGyro(json.getJSONObject("gyro"), warnings)
                } else {
                    GyroConfig()
                }
            val layout =
                TouchLayout(
                    version = json.optInt("version", 1),
                    name = json.optString("name", "Imported"),
                    globalOpacity =
                        json
                            .optDouble(
                                "globalOpacity",
                                TouchBindings.DEFAULT_GLOBAL_OPACITY.toDouble(),
                            ).toFloat(),
                    sticks = sticks,
                    buttons = buttons,
                    sliders = sliders,
                    radialMenus = radials,
                    dpads = dpads,
                    gyro = gyro,
                )
            return ParseResult(layout, warnings)
        } catch (e: Exception) {
            warnings.add("Failed to parse touch layout: ${e.message}")
            Log.e(TAG, "Touch layout parse failed", e)
            return ParseResult(null, warnings)
        }
    }

    private fun parseStick(
        j: JSONObject,
        warnings: MutableList<String>,
    ): AnalogStickControl? {
        val id = j.optString("id", "")
        val axisX = resolveAxis(j, "axisX", warnings, "stick '$id'") ?: return null
        val axisY = resolveAxis(j, "axisY", warnings, "stick '$id'") ?: return null
        val buttonMode = j.optBoolean("buttonMode")
        return AnalogStickControl(
            id = id,
            xPct = j.getDouble("x").toFloat(),
            yPct = j.getDouble("y").toFloat(),
            sizeMult = j.optDouble("size", 1.0).toFloat(),
            opacity = j.optDouble("opacity", 0.7).toFloat(),
            axisX = axisX,
            axisY = axisY,
            invertX = j.optBoolean("invertX"),
            invertY = j.optBoolean("invertY"),
            deadzone = j.optInt("deadzone", 15),
            responseCurve = ResponseCurve.valueOf(j.optString("responseCurve", "LINEAR")),
            exponent = j.optDouble("exponent", TouchBindings.DEFAULT_EXPONENT.toDouble()).toFloat(),
            sensitivity = j.optDouble("sensitivity", 1.0).toFloat(),
            floating = j.optBoolean("floating"),
            floatingZone =
                if (j.has("floatingZone")) {
                    FloatingZone.fromJson(j.getJSONObject("floatingZone"))
                } else {
                    FloatingZone()
                },
            hapticFeedback = j.optBoolean("haptic", true),
            buttonMode = buttonMode,
            negXBinding =
                if (buttonMode) {
                    resolveBinding(j, "negXBinding", warnings, "stick '$id'")
                        ?: TouchBindings.BTN_FIRE_PRIMARY
                } else {
                    TouchBindings.BTN_FIRE_PRIMARY
                },
            posXBinding =
                if (buttonMode) {
                    resolveBinding(j, "posXBinding", warnings, "stick '$id'")
                        ?: TouchBindings.BTN_FIRE_PRIMARY
                } else {
                    TouchBindings.BTN_FIRE_PRIMARY
                },
            negYBinding =
                if (buttonMode) {
                    resolveBinding(j, "negYBinding", warnings, "stick '$id'")
                        ?: TouchBindings.BTN_FIRE_PRIMARY
                } else {
                    TouchBindings.BTN_FIRE_PRIMARY
                },
            posYBinding =
                if (buttonMode) {
                    resolveBinding(j, "posYBinding", warnings, "stick '$id'")
                        ?: TouchBindings.BTN_FIRE_PRIMARY
                } else {
                    TouchBindings.BTN_FIRE_PRIMARY
                },
        )
    }

    private fun parseButton(
        j: JSONObject,
        warnings: MutableList<String>,
    ): ButtonControl? {
        val id = j.optString("id", "")
        val binding = resolveBinding(j, "binding", warnings, "button '$id'") ?: return null
        return ButtonControl(
            id = id,
            xPct = j.getDouble("x").toFloat(),
            yPct = j.getDouble("y").toFloat(),
            sizeMult = j.optDouble("size", 1.0).toFloat(),
            opacity = j.optDouble("opacity", 0.7).toFloat(),
            binding = binding,
            label = j.optString("label", ""),
            shape = ButtonShape.valueOf(j.optString("shape", "CIRCLE")),
            toggle = j.optBoolean("toggle"),
            hapticFeedback = j.optBoolean("haptic", true),
        )
    }

    private fun parseSlider(
        j: JSONObject,
        warnings: MutableList<String>,
    ): SliderControl? {
        val id = j.optString("id", "")
        val axis = resolveAxis(j, "axis", warnings, "slider '$id'") ?: return null
        return SliderControl(
            id = id,
            xPct = j.getDouble("x").toFloat(),
            yPct = j.getDouble("y").toFloat(),
            sizeMult = j.optDouble("size", 1.0).toFloat(),
            opacity = j.optDouble("opacity", 0.7).toFloat(),
            axis = axis,
            orientation = SliderOrientation.valueOf(j.optString("orientation", "VERTICAL")),
            responseCurve = ResponseCurve.valueOf(j.optString("responseCurve", "LINEAR")),
            exponent = j.optDouble("exponent", TouchBindings.DEFAULT_EXPONENT.toDouble()).toFloat(),
            sensitivity = j.optDouble("sensitivity", 1.0).toFloat(),
            springBack = j.optBoolean("springBack", true),
        )
    }

    private fun parseRadial(
        j: JSONObject,
        warnings: MutableList<String>,
    ): RadialMenuControl? {
        val id = j.optString("id", "")
        val segArr = j.optJSONArray("segments")
        if (segArr == null) {
            warnings.add("Radial menu '$id': missing segments array")
            return null
        }
        val segments = mutableListOf<RadialSegment>()
        for (i in 0 until segArr.length()) {
            val sj = segArr.getJSONObject(i)
            val binding = resolveBinding(sj, "binding", warnings, "radial '$id' segment $i")
            if (binding != null) {
                segments.add(
                    RadialSegment(
                        label = sj.getString("label"),
                        binding = binding,
                        iconRes = sj.optString("iconRes", ""),
                        weaponIndex = sj.optInt("wpnIdx", -1),
                    ),
                )
            }
        }
        val centerBinding =
            if (j.has("centerBinding")) {
                resolveBinding(j, "centerBinding", warnings, "radial '$id' center") ?: -1
            } else {
                -1
            }
        return RadialMenuControl(
            id = id,
            xPct = j.getDouble("x").toFloat(),
            yPct = j.getDouble("y").toFloat(),
            sizeMult = j.optDouble("size", 1.0).toFloat(),
            opacity = j.optDouble("opacity", 0.7).toFloat(),
            segments = segments,
            centerLabel = j.optString("centerLabel", ""),
            centerBinding = centerBinding,
            hapticFeedback = j.optBoolean("haptic", true),
        )
    }

    private fun parseDPad(
        j: JSONObject,
        warnings: MutableList<String>,
    ): DPadControl? {
        val id = j.optString("id", "")
        val up = resolveBinding(j, "up", warnings, "dpad '$id'") ?: return null
        val down = resolveBinding(j, "down", warnings, "dpad '$id'") ?: return null
        val left = resolveBinding(j, "left", warnings, "dpad '$id'") ?: return null
        val right = resolveBinding(j, "right", warnings, "dpad '$id'") ?: return null
        return DPadControl(
            id = id,
            xPct = j.getDouble("x").toFloat(),
            yPct = j.getDouble("y").toFloat(),
            sizeMult = j.optDouble("size", 1.0).toFloat(),
            opacity = j.optDouble("opacity", 0.7).toFloat(),
            mode = DPadMode.valueOf(j.optString("mode", "INDIVIDUAL")),
            upBinding = up,
            downBinding = down,
            leftBinding = left,
            rightBinding = right,
            swipeThreshold =
                j
                    .optDouble(
                        "swipeThreshold",
                        TouchBindings.DEFAULT_SWIPE_THRESHOLD.toDouble(),
                    ).toFloat(),
            hapticFeedback = j.optBoolean("haptic", true),
        )
    }

    private fun parseGyro(
        j: JSONObject,
        warnings: MutableList<String>,
    ): GyroConfig {
        val axisX =
            resolveAxis(j, "axisX", warnings, "gyro")
                ?: TouchBindings.AXIS_RIGHT_X
        val axisY =
            resolveAxis(j, "axisY", warnings, "gyro")
                ?: TouchBindings.AXIS_RIGHT_Y
        return GyroConfig(
            enabled = j.optBoolean("enabled"),
            activation = GyroActivation.valueOf(j.optString("activation", "ALWAYS")),
            sensitivityX = j.optDouble("sensitivityX", 1.0).toFloat(),
            sensitivityY = j.optDouble("sensitivityY", 1.0).toFloat(),
            invertX = j.optBoolean("invertX"),
            invertY = j.optBoolean("invertY"),
            axisX = axisX,
            axisY = axisY,
            deadzone = j.optDouble("deadzone", 0.02).toFloat(),
        )
    }

    // ── Binding / axis field resolution (string or integer) ──

    /** Resolve a field that may be a string name or an integer. Returns null on error. */
    private fun resolveBinding(
        j: JSONObject,
        key: String,
        warnings: MutableList<String>,
        context: String,
    ): Int? {
        if (!j.has(key)) {
            warnings.add("$context: missing required field '$key'")
            return null
        }
        val raw = j.get(key)
        if (raw is Int || raw is Long) return (raw as Number).toInt()
        if (raw is Number) return raw.toInt()
        val name = raw.toString()
        val resolved = TouchBindings.nameToBinding(name)
        if (resolved == null) {
            warnings.add("$context: unknown binding name '$name' for field '$key'")
        }
        return resolved
    }

    /** Resolve an axis field that may be a string name or an integer. */
    private fun resolveAxis(
        j: JSONObject,
        key: String,
        warnings: MutableList<String>,
        context: String,
    ): Int? {
        if (!j.has(key)) {
            warnings.add("$context: missing required field '$key'")
            return null
        }
        val raw = j.get(key)
        if (raw is Int || raw is Long) return (raw as Number).toInt()
        if (raw is Number) return raw.toInt()
        val name = raw.toString()
        val resolved = TouchBindings.nameToAxis(name)
        if (resolved == null) {
            warnings.add("$context: unknown axis name '$name' for field '$key'")
        }
        return resolved
    }

    /** Parse a JSON array, skipping elements that fail to parse. */
    private fun <T> parseArraySafe(
        parent: JSONObject,
        key: String,
        warnings: MutableList<String>,
        parse: (JSONObject, MutableList<String>) -> T?,
    ): List<T> {
        val arr = parent.optJSONArray(key) ?: return emptyList()
        val result = mutableListOf<T>()
        for (i in 0 until arr.length()) {
            try {
                val item = parse(arr.getJSONObject(i), warnings)
                if (item != null) result.add(item)
            } catch (e: Exception) {
                warnings.add("$key[$i]: parse error: ${e.message}")
            }
        }
        return result
    }

    // ── Controller Config: human-readable ──

    fun controllerConfigToHumanJson(
        bindings: Map<String, String>,
        inverts: Set<String>,
        thresholds: Map<String, Int> = emptyMap(),
    ): JSONObject {
        val j = JSONObject()
        j.put("type", "controller_config")
        j.put("version", 1)
        val bindingsObj = JSONObject()
        for ((k, v) in bindings) bindingsObj.put(k, v)
        j.put("bindings", bindingsObj)
        j.put("inverts", JSONArray(inverts.toList()))
        if (thresholds.isNotEmpty()) {
            val tObj = JSONObject()
            for ((k, v) in thresholds) tObj.put(k, v)
            j.put("thresholds", tObj)
        }
        return j
    }

    data class ControllerConfigData(
        val bindings: Map<String, String>,
        val inverts: Set<String>,
        val thresholds: Map<String, Int>,
    )

    fun humanJsonToControllerConfig(json: JSONObject): ParseResult<ControllerConfigData> {
        val warnings = mutableListOf<String>()
        try {
            if (!json.has("bindings")) {
                warnings.add("Controller config: missing 'bindings' object")
                return ParseResult(null, warnings)
            }
            val bindingsObj = json.getJSONObject("bindings")
            val bindings = mutableMapOf<String, String>()
            for (key in bindingsObj.keys()) {
                bindings[key] = bindingsObj.getString(key)
            }
            val inverts = mutableSetOf<String>()
            val invertsArr = json.optJSONArray("inverts")
            if (invertsArr != null) {
                for (i in 0 until invertsArr.length()) {
                    inverts.add(invertsArr.getString(i))
                }
            }
            val thresholds = mutableMapOf<String, Int>()
            val tObj = json.optJSONObject("thresholds")
            if (tObj != null) {
                for (key in tObj.keys()) thresholds[key] = tObj.getInt(key).coerceIn(5, 95)
            }
            return ParseResult(ControllerConfigData(bindings, inverts, thresholds), warnings)
        } catch (e: Exception) {
            warnings.add("Controller config parse failed: ${e.message}")
            Log.e(TAG, "Controller config parse failed", e)
            return ParseResult(null, warnings)
        }
    }

    // ── Config type detection ──

    /** Detect the type of a config JSON by its "type" field or by structure. */
    fun detectConfigType(json: JSONObject): String {
        val type = json.optString("type", "")
        if (type.isNotEmpty()) return type
        // Infer from structure
        if (json.has("sticks") || json.has("buttons") || json.has("radialMenus")) {
            return "touch_layout"
        }
        if (json.has("bindings")) return "controller_config"
        return "unknown"
    }
}
