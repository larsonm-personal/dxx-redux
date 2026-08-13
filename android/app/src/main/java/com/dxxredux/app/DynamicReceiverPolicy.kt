package com.dxxredux.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.IntentFilter
import androidx.core.content.ContextCompat

internal object DynamicReceiverPolicy {
    internal fun appInternalOrDebugExternalFlags(debug: Boolean): Int =
        if (debug) ContextCompat.RECEIVER_EXPORTED else ContextCompat.RECEIVER_NOT_EXPORTED

    internal fun debugExternalFlags(debug: Boolean): Int? = if (debug) ContextCompat.RECEIVER_EXPORTED else null

    fun registerAppInternalOrDebugExternal(
        context: Context,
        receiver: BroadcastReceiver,
        filter: IntentFilter,
    ) {
        ContextCompat.registerReceiver(
            context,
            receiver,
            filter,
            appInternalOrDebugExternalFlags(BuildConfig.DEBUG),
        )
    }

    fun registerDebugExternal(
        context: Context,
        receiver: BroadcastReceiver,
        filter: IntentFilter,
    ): Boolean {
        val flags = debugExternalFlags(BuildConfig.DEBUG) ?: return false
        ContextCompat.registerReceiver(context, receiver, filter, flags)
        return true
    }

    fun registerAppInternal(
        context: Context,
        receiver: BroadcastReceiver,
        filter: IntentFilter,
    ) {
        ContextCompat.registerReceiver(context, receiver, filter, ContextCompat.RECEIVER_NOT_EXPORTED)
    }
}
