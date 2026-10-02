package com.dxxredux.app.multiplayer

import android.content.Context
import android.net.ConnectivityManager
import android.net.LinkProperties
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.os.Build
import android.os.Handler
import android.os.Looper
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.net.Inet4Address
import java.net.NetworkInterface

internal data class LanHostAddress(
    val address: String,
    val interfaceName: String,
) {
    val label: String get() = "$interfaceName: $address"
}

/** Observes physical LANs, including offline Wi-Fi and hotspot interfaces */
internal class LanHostAddresses(
    context: Context,
) {
    private val connectivity = context.applicationContext.getSystemService(ConnectivityManager::class.java)
    private val handler = Handler(Looper.getMainLooper())
    private val mutableAddresses = MutableStateFlow<List<LanHostAddress>>(emptyList())
    val addresses = mutableAddresses.asStateFlow()
    var onChanged: ((List<LanHostAddress>) -> Unit)? = null
    var onNetworkLost: (() -> Unit)? = null
    private var running = false
    private var registered = false
    private val refresh =
        object : Runnable {
            override fun run() {
                if (!running) return
                val current = readAddresses()
                if (current != mutableAddresses.value) {
                    mutableAddresses.value = current
                    onChanged?.invoke(current)
                }
                handler.removeCallbacks(this)
                // Hotspot changes are not always represented as ConnectivityManager networks
                handler.postDelayed(this, 3_000)
            }
        }
    private val callback =
        object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) = changed()

            override fun onLost(network: Network) {
                handler.post { if (running) onNetworkLost?.invoke() }
                changed()
            }

            override fun onLinkPropertiesChanged(
                network: Network,
                properties: LinkProperties,
            ) = changed()

            override fun onCapabilitiesChanged(
                network: Network,
                capabilities: NetworkCapabilities,
            ) = changed()
        }

    private fun changed() {
        handler.post {
            handler.removeCallbacks(refresh)
            if (running) refresh.run()
        }
    }

    fun start() {
        if (running) return
        running = true
        try {
            val request = NetworkRequest.Builder()
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                request.clearCapabilities()
            } else {
                request.removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED)
                request.removeCapability(NetworkCapabilities.NET_CAPABILITY_TRUSTED)
                request.removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN)
            }
            connectivity.registerNetworkCallback(request.build(), callback)
            registered = true
        } catch (_: RuntimeException) {
            // Interface polling remains available if callback registration fails
        }
        refresh.run()
    }

    fun stop() {
        running = false
        handler.removeCallbacks(refresh)
        if (registered) {
            try {
                connectivity.unregisterNetworkCallback(callback)
            } catch (_: RuntimeException) {
            }
            registered = false
        }
    }

    // The snapshot also seeds interface exclusions before the first network callback
    @Suppress("DEPRECATION")
    private fun readAddresses(): List<LanHostAddress> =
        try {
            val physical = mutableSetOf<String>()
            val wifi = mutableSetOf<String>()
            val excluded = mutableSetOf<String>()
            val primaryAddresses = mutableSetOf<String>()
            val activeInterface = connectivity.activeNetwork?.let { connectivity.getLinkProperties(it)?.interfaceName }
            connectivity.allNetworks.forEach { network ->
                val capabilities = connectivity.getNetworkCapabilities(network) ?: return@forEach
                val properties = connectivity.getLinkProperties(network) ?: return@forEach
                val name = properties.interfaceName ?: return@forEach
                if (capabilities.hasTransport(NetworkCapabilities.TRANSPORT_VPN) ||
                    capabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)
                ) {
                    excluded += name
                } else if (capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) ||
                    capabilities.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)
                ) {
                    physical += name
                    properties.linkAddresses
                        .firstOrNull { it.address is Inet4Address }
                        ?.address
                        ?.hostAddress
                        ?.let(primaryAddresses::add)
                    if (capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) wifi += name
                }
            }
            NetworkInterface
                .getNetworkInterfaces()
                ?.toList()
                .orEmpty()
                .filter { iface ->
                    iface.isUp && !iface.isLoopback && iface.name !in excluded &&
                        (iface.name in physical || physicalInterface.matches(iface.name))
                }.flatMap { iface ->
                    iface.inetAddresses.toList().filterIsInstance<Inet4Address>().mapNotNull { ip ->
                        ip.hostAddress?.let(LanInvitation::validAddress)?.let { LanHostAddress(it, iface.name) }
                    }
                }.distinct()
                .sortedWith(
                    compareBy(
                        {
                            if (it.interfaceName == activeInterface) {
                                0
                            } else if (it.interfaceName in wifi) {
                                1
                            } else {
                                2
                            }
                        },
                        { it.interfaceName },
                        { if (it.address in primaryAddresses) 0 else 1 },
                        { it.address },
                    ),
                )
        } catch (_: Exception) {
            emptyList()
        }

    companion object {
        const val EXTRA_ADDRESS = "lan_qr_address"
        private const val PREFS = "lan_qr"
        private val physicalInterface = Regex("(?:wlan|wifi|wl|ap|swlan|softap|eth|en|rndis|usb|br)[a-zA-Z0-9_.-]*")

        fun preferred(context: Context): String? =
            context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getString("address", null)

        fun select(
            context: Context,
            address: String,
        ) {
            context
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                .edit()
                .putString("address", address)
                .apply()
        }

        fun broadcastAddresses(): Set<String> =
            try {
                NetworkInterface
                    .getNetworkInterfaces()
                    ?.toList()
                    .orEmpty()
                    .flatMap { it.interfaceAddresses }
                    .mapNotNull { it.broadcast?.hostAddress }
                    .toSet()
            } catch (_: Exception) {
                emptySet()
            }
    }
}
