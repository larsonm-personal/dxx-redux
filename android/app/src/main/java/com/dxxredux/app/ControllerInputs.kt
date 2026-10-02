package com.dxxredux.app

import android.view.MotionEvent

// IDs match android_virtual_gamepad.h; 6/7 are gyro and 8-10 are combiners
internal const val CONTROLLER_SAMPLE_AXIS_COUNT = 13
internal val CONTROLLER_COMBINER_AXES = listOf(8, 9, 10, 13, 14, 15)
internal val CONTROLLER_MOTION_AXES =
    linkedMapOf(
        "LS_X" to MotionEvent.AXIS_X,
        "LS_Y" to MotionEvent.AXIS_Y,
        "RS_X" to MotionEvent.AXIS_Z,
        "RS_Y" to MotionEvent.AXIS_RZ,
        "LT" to MotionEvent.AXIS_LTRIGGER,
        "RT" to MotionEvent.AXIS_RTRIGGER,
        "BRAKE" to MotionEvent.AXIS_BRAKE,
        "GAS" to MotionEvent.AXIS_GAS,
    )

// Every Android source remains independently bindable, even when values coincide
internal fun readControllerAxes(
    event: MotionEvent,
    values: FloatArray,
) {
    for ((control, motionAxis) in CONTROLLER_MOTION_AXES) {
        values[AXIS_CONTROLS.getValue(control)] = event.getAxisValue(motionAxis)
    }
}
