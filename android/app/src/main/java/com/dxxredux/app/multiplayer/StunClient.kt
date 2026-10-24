package com.dxxredux.app.multiplayer

import android.util.Log
import java.net.Inet4Address
import java.net.NetworkInterface

private const val TAG = "StunClient"

object StunClient {
    /** Get non-loopback IPv4 addresses for host candidates. */
    internal fun getLocalIpv4Addresses(): List<String> {
        val addrs = mutableListOf<String>()
        try {
            for (iface in NetworkInterface.getNetworkInterfaces().toList()) {
                if (iface.isLoopback || !iface.isUp) continue
                for (addr in iface.inetAddresses.toList()) {
                    if (addr is Inet4Address && !addr.isLoopbackAddress) {
                        addr.hostAddress?.let { addrs.add(it) }
                    }
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to enumerate network interfaces: ${e.message}")
        }
        return addrs
    }
}
