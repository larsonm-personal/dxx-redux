package com.dxxredux.app

import android.view.InputDevice

internal data class ControllerDisplayDevice(
    val id: Int,
    val name: String,
    val vendorId: Int,
    val productId: Int,
    val isVirtual: Boolean,
)

internal fun controllerDisplayDevice(device: InputDevice): ControllerDisplayDevice =
    ControllerDisplayDevice(
        id = device.id,
        name = device.name,
        vendorId = device.vendorId,
        productId = device.productId,
        isVirtual = device.isVirtual,
    )

private fun hasHardwareIds(device: ControllerDisplayDevice): Boolean = device.vendorId != 0 || device.productId != 0

private fun isGenericControllerName(name: String): Boolean {
    val normalized = name.trim().lowercase()
    return normalized.isEmpty() ||
        normalized == "virtual" ||
        normalized == "virtual controller" ||
        normalized == "virtual gamepad" ||
        normalized == "controller" ||
        normalized == "gamepad" ||
        normalized == "input device"
}

internal fun selectDisplayedController(devices: List<ControllerDisplayDevice>): ControllerDisplayDevice? =
    devices
        .sortedWith(
            compareByDescending<ControllerDisplayDevice> { !it.isVirtual }
                .thenByDescending { hasHardwareIds(it) }
                .thenByDescending { !isGenericControllerName(it.name) }
                .thenByDescending { it.name.length }
                .thenBy { it.id },
        ).firstOrNull()
