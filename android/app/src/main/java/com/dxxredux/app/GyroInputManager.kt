package com.dxxredux.app

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import kotlin.math.abs
import kotlin.math.sign

/**
 * Manages gyroscope-based aiming via TYPE_GAME_ROTATION_VECTOR.
 *
 * Uses the game rotation vector sensor (gyro + accelerometer, no
 * magnetometer) for gravity-referenced pitch/roll and smooth relative yaw
 * without magnetic interference. Extracts yaw/pitch/roll via
 * SensorManager.getOrientation() and outputs delta values as axis input.
 *
 * All three axes are available for mapping to game controls:
 *   yaw   (azimuth) -- relative horizontal rotation (drifts slowly)
 *   pitch           -- gravity-referenced forward/back tilt
 *   roll            -- gravity-referenced left/right tilt
 *
 * Lifecycle: call [resume] in onResume and [pause] in onStop/onPause.
 */
class GyroInputManager(
    context: Context,
) : SensorEventListener {
    private val sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private val sensor: Sensor? = sensorManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR)

    /** Called with (axisIndex, value) when gyro produces input. */
    var axisCallback: ((Int, Float) -> Unit)? = null

    /** Called with (yaw, pitch, roll) raw clamped values for diagnostic display. */
    var diagnosticCallback: ((Float, Float, Float) -> Unit)? = null

    /** Called with (azimuth, pitch, roll) when a new reference orientation is established. */
    var onCalibrated: ((Float, Float, Float) -> Unit)? = null

    // Current config (updated via setConfig)
    private var config = GyroConfig()

    // Orientation tracking
    private var hasReference = false
    private val curRotationMatrix = FloatArray(9)
    private val curOrientation = FloatArray(3) // [azimuth, pitch, roll]
    private val refOrientation = FloatArray(3)

    // Activation state
    var rightStickActive = false // set by TouchOverlayView when right stick is touched
    var toggledOn = false // toggled by a dedicated button

    /** Whether this device has a usable gyro sensor. */
    val isAvailable: Boolean get() = sensor != null

    fun setConfig(gyroConfig: GyroConfig) {
        val wasEnabled = config.enabled
        config = gyroConfig
        // Restore persisted reference orientation if available
        if (gyroConfig.refAzimuth != null && gyroConfig.refPitch != null && gyroConfig.refRoll != null) {
            refOrientation[0] = gyroConfig.refAzimuth
            refOrientation[1] = gyroConfig.refPitch
            refOrientation[2] = gyroConfig.refRoll
            hasReference = true
        }
        if (wasEnabled && !config.enabled) pause()
    }

    fun resume() {
        if (!config.enabled || sensor == null) return
        // Only clear reference if setConfig didn't load persisted values
        if (!hasReference) hasReference = false
        sensorManager.registerListener(this, sensor, SensorManager.SENSOR_DELAY_GAME)
    }

    fun pause() {
        sensorManager.unregisterListener(this)
        hasReference = false
    }

    /** Reset reference orientation to current position (recalibrate). */
    fun calibrate() {
        hasReference = false
    }

    override fun onAccuracyChanged(
        sensor: Sensor?,
        accuracy: Int,
    ) {}

    override fun onSensorChanged(event: SensorEvent) {
        if (event.sensor.type != Sensor.TYPE_GAME_ROTATION_VECTOR) return

        // Check activation mode
        val active =
            when (config.activation) {
                GyroActivation.ALWAYS -> true
                GyroActivation.TOUCH_STICK -> rightStickActive
                GyroActivation.ADS_ONLY -> toggledOn
            }

        SensorManager.getRotationMatrixFromVector(curRotationMatrix, event.values)
        SensorManager.getOrientation(curRotationMatrix, curOrientation)
        // curOrientation: [0]=azimuth (yaw), [1]=pitch, [2]=roll (all radians)

        if (!hasReference) {
            curOrientation.copyInto(refOrientation)
            hasReference = true
            onCalibrated?.invoke(refOrientation[0], refOrientation[1], refOrientation[2])
            return
        }

        if (!active) {
            // In RATE mode, update reference so there's no jump when activated.
            // In ABSOLUTE mode, keep reference fixed so tilt angle is preserved.
            if (config.mode == GyroMode.RATE) {
                curOrientation.copyInto(refOrientation)
            }
            diagnosticCallback?.invoke(0f, 0f, 0f)
            return
        }

        // Compute deltas (handle yaw wrap-around at +/- PI)
        var dYaw = normalizeAngle(curOrientation[0] - refOrientation[0])
        var dPitch = curOrientation[1] - refOrientation[1]
        var dRoll = curOrientation[2] - refOrientation[2]

        if (config.mode == GyroMode.RATE) {
            // Incremental: update reference each frame, output is angular velocity
            curOrientation.copyInto(refOrientation)
        }
        // ABSOLUTE mode: reference stays fixed, output proportional to tilt angle

        // Apply deadzone (convert fraction-of-maxAngle to radians per axis)
        val dzRadX = config.deadzoneX * config.maxAngleX
        val dzRadY = config.deadzoneY * config.maxAngleY
        val dzRadZ = config.deadzoneZ * config.maxAngleZ
        dYaw = applyDeadzone(dYaw, dzRadX)
        dPitch = applyDeadzone(dPitch, dzRadY)
        dRoll = applyDeadzone(dRoll, dzRadZ)

        // Scale by per-axis tilt range (used for both ABSOLUTE and RATE modes)
        val rangeX = (config.maxAngleX - dzRadX).coerceAtLeast(0.01f)
        val rangeY = (config.maxAngleY - dzRadY).coerceAtLeast(0.01f)
        val rangeZ = (config.maxAngleZ - dzRadZ).coerceAtLeast(0.01f)
        var outX = dYaw / rangeX
        var outY = dPitch / rangeY
        var outZ = dRoll / rangeZ
        if (config.invertX) outX = -outX
        if (config.invertY) outY = -outY
        if (config.invertZ) outZ = -outZ

        // Clamp to -1..1
        outX = outX.coerceIn(-1f, 1f)
        outY = outY.coerceIn(-1f, 1f)
        outZ = outZ.coerceIn(-1f, 1f)

        // Diagnostic callback (fires even if all zero, for live display)
        diagnosticCallback?.invoke(outX, outY, outZ)

        // Send to game axes (skip disabled axes)
        if (config.axisX >= 0) axisCallback?.invoke(config.axisX, outX)
        if (config.axisY >= 0) axisCallback?.invoke(config.axisY, outY)
        if (config.axisZ >= 0) axisCallback?.invoke(config.axisZ, outZ)
    }

    /** Normalize an angle delta to -PI..PI for yaw wrap-around. */
    private fun normalizeAngle(a: Float): Float {
        var v = a
        while (v > Math.PI.toFloat()) v -= (2 * Math.PI).toFloat()
        while (v < -Math.PI.toFloat()) v += (2 * Math.PI).toFloat()
        return v
    }

    private fun applyDeadzone(
        value: Float,
        deadzone: Float,
    ): Float {
        val a = abs(value)
        if (a < deadzone) return 0f
        return sign(value) * (a - deadzone) / (1f - deadzone)
    }
}
