package com.dxxredux.app.lobby

import android.content.Context
import android.net.ConnectivityManager
import android.net.LinkProperties
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.dxxredux.app.multiplayer.NetLog
import com.dxxredux.app.multiplayer.NetworkConstants
import java.net.Inet4Address
import java.util.concurrent.Executor

/** DNS-SD supplies candidates only; the lobby protocol confirms their identity and liveness */
internal class LanNsdDiscovery(
    private val endpointsChanged: (String, String, List<String>) -> Unit,
    private val endpointsCleared: () -> Unit,
    private val legacyResolution: () -> Boolean = { false },
) {
    companion object {
        const val SERVICE_TYPE = "_dxxredux._udp."
        private const val TAG = "LanNsdDiscovery"
        private const val MAX_SERVICES = 32
    }

    // All platform callbacks and lifecycle changes are serialized on the main looper
    private val handler = Handler(Looper.getMainLooper())
    private val executor = Executor { handler.post(it) }
    private var context: Context? = null
    private var hostId: String? = null
    private var callsign = ""
    private var session: Session? = null
    private var networkCallback: ConnectivityManager.NetworkCallback? = null
    private val networkAddresses = mutableMapOf<Network, String>()
    private val refresh = Runnable { restart() }

    fun start(
        context: Context,
        hostId: String?,
        callsign: String,
    ) {
        handler.post {
            val changed = this.context == null || this.hostId != hostId || this.callsign != callsign
            this.context = context.applicationContext
            this.hostId = hostId
            this.callsign = callsign
            if (networkCallback == null) watchNetworks()
            if (changed || session == null) scheduleRestart(0)
        }
    }

    fun stop() {
        handler.post {
            handler.removeCallbacks(refresh)
            session?.close()
            session = null
            networkCallback?.let {
                try {
                    context?.getSystemService(ConnectivityManager::class.java)?.unregisterNetworkCallback(it)
                } catch (e: RuntimeException) {
                    log("Network callback cleanup failed: ${e.message}")
                }
            }
            networkCallback = null
            networkAddresses.clear()
            context = null
            endpointsCleared()
        }
    }

    private fun watchNetworks() {
        val manager = context?.getSystemService(ConnectivityManager::class.java) ?: return
        @Suppress("DEPRECATION")
        manager.allNetworks
            .filter {
                manager.getNetworkCapabilities(it)?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
            }.forEach { network ->
                networkAddresses[network] = manager.getLinkProperties(network)?.linkAddresses.toString()
            }
        val callback =
            object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) = changed(network, manager.getLinkProperties(network))

                override fun onLost(network: Network) = changed(network, null)

                override fun onLinkPropertiesChanged(
                    network: Network,
                    linkProperties: LinkProperties,
                ) = changed(network, linkProperties)

                private fun changed(
                    network: Network,
                    properties: LinkProperties?,
                ) {
                    handler.post {
                        if (networkCallback !== this) return@post
                        val addresses = properties?.linkAddresses?.toString()
                        val previous =
                            if (addresses == null) {
                                networkAddresses.remove(network)
                            } else {
                                networkAddresses.put(network, addresses)
                            }
                        if (previous != addresses) scheduleRestart(1000)
                    }
                }
            }
        try {
            manager.registerNetworkCallback(
                NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build(),
                callback,
            )
            networkCallback = callback
        } catch (e: RuntimeException) {
            log("Network monitoring unavailable: ${e.message}")
        }
    }

    private fun scheduleRestart(delayMs: Long = 5000) {
        handler.removeCallbacks(refresh)
        if (context != null) handler.postDelayed(refresh, delayMs)
    }

    private fun restart() {
        val ctx = context ?: return
        session?.close()
        session = null
        endpointsCleared()
        val manager = ctx.getSystemService(NsdManager::class.java) ?: return
        session = Session(manager, hostId, callsign).also { it.open() }
    }

    private fun log(message: String) {
        Log.i(TAG, message)
        NetLog.log("NSD", message)
    }

    private inner class Session(
        val manager: NsdManager,
        val ownId: String?,
        val name: String,
    ) {
        var active = true
        var registered = false
        var discovering = false
        val services = mutableMapOf<String, NsdServiceInfo>()
        val callbacks = mutableMapOf<String, NsdManager.ServiceInfoCallback>()
        val pending = ArrayDeque<Pair<String, NsdServiceInfo>>()
        var resolving = false

        private fun key(info: NsdServiceInfo): String =
            if (Build.VERSION.SDK_INT >= 33) "${info.network}/${info.serviceName}" else info.serviceName

        val registration =
            object : NsdManager.RegistrationListener {
                override fun onServiceRegistered(info: NsdServiceInfo) {
                    handler.post {
                        registered = true
                        if (!active) {
                            unregister()
                        } else {
                            log(
                                "Registered lobby=$ownId name=${info.serviceName} port=${NetworkConstants.LAN_LOBBY_PORT}",
                            )
                        }
                    }
                }

                override fun onRegistrationFailed(
                    info: NsdServiceInfo,
                    errorCode: Int,
                ) {
                    handler.post {
                        if (active) {
                            log("Registration failed code=$errorCode")
                            scheduleRestart()
                        }
                    }
                }

                override fun onServiceUnregistered(info: NsdServiceInfo) {
                    handler.post { registered = false }
                }

                override fun onUnregistrationFailed(
                    info: NsdServiceInfo,
                    errorCode: Int,
                ) {
                    handler.post { log("Unregistration failed code=$errorCode") }
                }
            }

        val discovery =
            object : NsdManager.DiscoveryListener {
                override fun onDiscoveryStarted(type: String) {
                    handler.post { if (active) log("Browsing $type") }
                }

                override fun onDiscoveryStopped(type: String) {
                    handler.post { if (active) scheduleRestart() }
                }

                override fun onStartDiscoveryFailed(
                    type: String,
                    errorCode: Int,
                ) {
                    handler.post {
                        if (active) {
                            log("Browse failed code=$errorCode")
                            scheduleRestart()
                        }
                    }
                }

                override fun onStopDiscoveryFailed(
                    type: String,
                    errorCode: Int,
                ) {
                    handler.post { log("Stop browse failed code=$errorCode") }
                }

                override fun onServiceFound(info: NsdServiceInfo) {
                    handler.post {
                        if (!active || info.serviceType.trimEnd('.') != SERVICE_TYPE.trimEnd('.')) return@post
                        val key = key(info)
                        if (key in services || services.size >= MAX_SERVICES) return@post
                        services[key] = info
                        log("Found service=${info.serviceName}")
                        if (Build.VERSION.SDK_INT >= 34 && !legacyResolution()) {
                            track(key, info)
                        } else {
                            pending.addLast(key to info)
                            resolveNext()
                        }
                    }
                }

                override fun onServiceLost(info: NsdServiceInfo) {
                    handler.post {
                        if (!active) return@post
                        val key = key(info)
                        services.remove(key)
                        pending.removeAll { it.first == key }
                        if (Build.VERSION.SDK_INT >= 34) callbacks.remove(key)?.let { untrack(it) }
                        endpointsChanged(key, "", emptyList())
                        log("Lost service=${info.serviceName}")
                    }
                }
            }

        fun open() {
            try {
                if (ownId != null) {
                    val info =
                        NsdServiceInfo().apply {
                            serviceName = "DXX ${name.take(24)} ${ownId.take(8)}"
                            serviceType = SERVICE_TYPE
                            port = NetworkConstants.LAN_LOBBY_PORT
                            setAttribute("v", LAN_LOBBY_PROTOCOL_VERSION.toString())
                            setAttribute("id", ownId)
                        }
                    manager.registerService(info, NsdManager.PROTOCOL_DNS_SD, registration)
                }
                manager.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, discovery)
                discovering = true
            } catch (e: RuntimeException) {
                log("Start failed: ${e.message}")
                scheduleRestart()
            }
        }

        fun resolved(
            key: String,
            info: NsdServiceInfo,
        ) {
            if (!active || key !in services) return
            val id = info.attributes["id"]?.toString(Charsets.UTF_8).orEmpty()
            val version = info.attributes["v"]?.toString(Charsets.UTF_8)
            if (id.isBlank() || id == ownId || version != LAN_LOBBY_PROTOCOL_VERSION.toString() ||
                info.port != NetworkConstants.LAN_LOBBY_PORT
            ) {
                endpointsChanged(key, "", emptyList())
                return
            }
            @Suppress("DEPRECATION")
            val hosts = if (Build.VERSION.SDK_INT >= 34) info.hostAddresses else listOfNotNull(info.host)
            val addresses =
                hosts
                    .filterIsInstance<Inet4Address>()
                    .filter { !it.isLoopbackAddress && !it.isAnyLocalAddress && !it.isMulticastAddress }
                    .mapNotNull { it.hostAddress }
                    .distinct()
            endpointsChanged(key, id, addresses)
            if (addresses.isNotEmpty()) log("Resolved lobby=$id addresses=$addresses port=${info.port}")
        }

        @android.annotation.TargetApi(34)
        fun track(
            key: String,
            info: NsdServiceInfo,
        ) {
            val callback =
                object : NsdManager.ServiceInfoCallback {
                    override fun onServiceUpdated(serviceInfo: NsdServiceInfo) {
                        if (services[key] === info) resolved(key, serviceInfo)
                    }

                    override fun onServiceLost() {
                        if (active && services[key] === info) endpointsChanged(key, "", emptyList())
                    }

                    override fun onServiceInfoCallbackUnregistered() {}

                    override fun onServiceInfoCallbackRegistrationFailed(errorCode: Int) {
                        if (active) {
                            log("Track failed code=$errorCode")
                            scheduleRestart()
                        }
                    }
                }
            callbacks[key] = callback
            try {
                manager.registerServiceInfoCallback(info, executor, callback)
            } catch (
                e: RuntimeException,
            ) {
                log("Track failed: ${e.message}")
                scheduleRestart()
            }
        }

        @Suppress("DEPRECATION")
        fun resolveNext() {
            if (!active || resolving || pending.isEmpty()) return
            val (key, info) = pending.removeFirst()
            resolving = true
            log("Resolving legacy service=${info.serviceName}")
            // Older NSD implementations only support one outstanding resolution
            try {
                manager.resolveService(
                    info,
                    object : NsdManager.ResolveListener {
                        override fun onServiceResolved(serviceInfo: NsdServiceInfo) {
                            handler.post {
                                resolving = false
                                if (services[key] === info) {
                                    resolved(key, serviceInfo)
                                    handler.postDelayed({
                                        if (active && services[key] === info) {
                                            pending.addLast(key to info)
                                            resolveNext()
                                        }
                                    }, 15000)
                                }
                                resolveNext()
                            }
                        }

                        override fun onResolveFailed(
                            serviceInfo: NsdServiceInfo,
                            errorCode: Int,
                        ) {
                            handler.post {
                                resolving = false
                                if (active && services[key] === info) {
                                    log("Resolve failed code=$errorCode")
                                    scheduleRestart()
                                }
                                resolveNext()
                            }
                        }
                    },
                )
            } catch (e: RuntimeException) {
                resolving = false
                log("Resolve failed: ${e.message}")
                scheduleRestart()
            }
        }

        @android.annotation.TargetApi(34)
        fun untrack(callback: NsdManager.ServiceInfoCallback) {
            try {
                manager.unregisterServiceInfoCallback(callback)
            } catch (
                e: RuntimeException,
            ) {
                log("Track cleanup failed: ${e.message}")
            }
        }

        fun unregister() {
            if (!registered) return
            registered = false
            try {
                manager.unregisterService(registration)
            } catch (
                e: RuntimeException,
            ) {
                log("Advertisement cleanup failed: ${e.message}")
            }
        }

        fun close() {
            active = false
            if (discovering) {
                try {
                    manager.stopServiceDiscovery(discovery)
                } catch (
                    e: RuntimeException,
                ) {
                    log("Browse cleanup failed: ${e.message}")
                }
            }
            if (Build.VERSION.SDK_INT >= 34) callbacks.values.forEach { untrack(it) }
            callbacks.clear()
            services.clear()
            pending.clear()
            unregister()
        }
    }
}
