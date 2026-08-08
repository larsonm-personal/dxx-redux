package com.dxxredux.app

/**
 * JNI bridge for meta action dispatch (android_meta_actions.c).
 * Meta actions inject SDL key sequences for game functions that
 * aren't part of kc_joystick[] (quicksave, guide bot, etc.).
 */
object NativeMetaActions {
    init {
        // Both D1 and D2 native libs include android_meta_actions.c.
        // The active library is already loaded by NativePilotPatcher
        // or MainActivity before any meta action calls.
    }

    /**
     * Dispatch a meta action by injecting the corresponding SDL key sequence.
     *
     * @param actionId one of the META_* constants from TouchBindings
     * @param pressed  1 = button down, 0 = button up
     * @return 0 on success, -1 if actionId is unknown
     */
    @JvmStatic
    external fun nativeMetaAction(
        actionId: Int,
        pressed: Int,
    ): Int

    /** Queue exact full-index weapon selection for the game thread. */
    @JvmStatic
    external fun nativeSelectWeaponExact(
        weaponClass: Int,
        weaponIndex: Int,
    ): Int
}
