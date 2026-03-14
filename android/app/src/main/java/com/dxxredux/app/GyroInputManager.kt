package com.dxxredux.app

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import kotlin.math.abs
import kotlin.math.asin
import kotlin.math.atan2
import kotlin.math.sign

/**
 * Manages gyroscope-based aiming via TYPE_ROTATION_VECTOR.
 *
 * Uses the gravity-referenced rotation vector sensor so tilt angles are
 * stable and absolute (no heading drift). Computes yaw/pitch deltas from
 * phone orientation changes and outputs them as axis values via
 * [axisCallback]. Designed to layer on top of stick input -- the caller
 * sums stick + gyro values before sending to JNI.
 *
 * Lifecycle: call [resume] in onResume and [pause] in onStop/onPause.
 */
class GyroInputManager(
    context: Context,
) : SensorEventListener {
    private val sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private val sensor: Sensor? = sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)

    /** Called with (axisIndex, deltaValue) when gyro produces input. */
    var axisCallback: ((Int, Float) -> Unit)? = null

    // Current config (updated via setConfig)
    private var config = GyroConfig()

    // Orientation tracking
    private var hasReference = false
    private val refRotationMatrix = FloatArray(9)
    private val curRotationMatrix = FloatArray(9)
    private val deltaRotationMatrix = FloatArray(9)
    private val orientationAngles = FloatArray(3)

    // Activation state
    var rightStickActive = false // set by TouchOverlayView when right stick is touched
    var toggledOn = false // toggled by a dedicated button

    /** Whether this device has a usable gyro sensor. */
    val isAvailable: Boolean get() = sensor != null

    fun setConfig(gyroConfig: GyroConfig) {
        val wasEnabled = config.enabled
        config = gyroConfig
        if (wasEnabled && !config.enabled) pause()
    }

    fun resume() {
        if (!config.enabled || sensor == null) return
        hasReference = false
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
        if (event.sensor.type != Sensor.TYPE_ROTATION_VECTOR) return

        // Check activation mode
        val active =
            when (config.activation) {
                GyroActivation.ALWAYS -> true
                GyroActivation.TOUCH_STICK -> rightStickActive
                GyroActivation.ADS_ONLY -> toggledOn
            }

        SensorManager.getRotationMatrixFromVector(curRotationMatrix, event.values)

        if (!hasReference) {
            refRotationMatrix.indices.forEach { refRotationMatrix[it] = curRotationMatrix[it] }
            hasReference = true
            return
        }

        if (!active) {
            // Not active — update reference so there's no jump when activated
            refRotationMatrix.indices.forEach { refRotationMatrix[it] = curRotationMatrix[it] }
            return
        }

        // Compute delta: deltaR = refR^T * curR
        // refR is 3x3 row-major, transpose means swap row/col indices
        for (r in 0..2) {
            for (c in 0..2) {
                deltaRotationMatrix[r * 3 + c] =
                    refRotationMatrix[0 * 3 + r] * curRotationMatrix[0 * 3 + c] +
                    refRotationMatrix[1 * 3 + r] * curRotationMatrix[1 * 3 + c] +
                    refRotationMatrix[2 * 3 + r] * curRotationMatrix[2 * 3 + c]
            }
        }

        // Extract yaw (azimuth) and pitch from the delta rotation
        val pitch = asin((-deltaRotationMatrix[7]).toDouble().coerceIn(-1.0, 1.0)).toFloat()
        val yaw = atan2(deltaRotationMatrix[6].toDouble(), deltaRotationMatrix[8].toDouble()).toFloat()

        if (config.mode == GyroMode.RATE) {
            // Incremental: update reference each frame, output is angular velocity
            refRotationMatrix.indices.forEach { refRotationMatrix[it] = curRotationMatrix[it] }
        }
        // ABSOLUTE mode: reference stays fixed, output is proportional to tilt angle

        // Apply deadzone
        val dzX = applyDeadzone(yaw, config.deadzone)
        val dzY = applyDeadzone(pitch, config.deadzone)
        if (dzX == 0f && dzY == 0f) return

        // Apply sensitivity and inversion
        var outX: Float
        var outY: Float
        if (config.mode == GyroMode.ABSOLUTE) {
            // Map angle beyond deadzone proportionally within maxAngle
            val range = config.maxAngle - config.deadzone
            outX = if (range > 0f) (dzX / range) else dzX
            outY = if (range > 0f) (dzY / range) else dzY
            outX *= config.sensitivityX
            outY *= config.sensitivityY
        } else {
            outX = dzX * config.sensitivityX
            outY = dzY * config.sensitivityY
        }
        if (config.invertX) outX = -outX
        if (config.invertY) outY = -outY

        // Clamp to -1..1
        outX = outX.coerceIn(-1f, 1f)
        outY = outY.coerceIn(-1f, 1f)

        axisCallback?.invoke(config.axisX, outX)
        axisCallback?.invoke(config.axisY, outY)
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
