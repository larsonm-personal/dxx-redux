package com.dxxredux.app.multiplayer

import android.util.Log
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.HttpURLConnection
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.URL

private const val TAG = "UpnpClient"

// SSDP multicast address and port (UPnP Device Architecture 1.0)
private const val SSDP_ADDR = "239.255.255.250"
private const val SSDP_PORT = 1900
private const val SSDP_TIMEOUT_MS = 2000
private const val HTTP_TIMEOUT_MS = 3000
private const val UPNP_LEASE_SECONDS = 3600 // 1 hour lease; game sessions are shorter

data class UpnpMapping(
    val externalIp: String,
    val externalPort: Int,
    val internalPort: Int,
    val controlUrl: String,
    val serviceType: String,
)

/**
 * Minimal UPnP IGD (Internet Gateway Device) client for port mapping.
 *
 * Discovers the local router's UPnP service, maps a UDP port, and returns
 * the external IP:port so it can be used as a "upnp" connection candidate.
 * Runs blocking I/O -- call from Dispatchers.IO.
 */
object UpnpClient {
    /**
     * Attempt to map [internalPort] (UDP) via UPnP and return the mapping info.
     * Returns null if UPnP is unavailable, the router doesn't support it,
     * or the mapping fails.
     */
    fun tryMap(
        internalPort: Int,
        localIp: String,
    ): UpnpMapping? {
        return try {
            val location = discoverGateway() ?: return null
            Log.d(TAG, "IGD location: $location")

            val (controlUrl, serviceType) = parseDeviceDescription(location) ?: return null
            Log.d(TAG, "Control URL: $controlUrl, service: $serviceType")

            val externalIp = getExternalIp(controlUrl, serviceType) ?: return null
            Log.d(TAG, "External IP: $externalIp")

            val mapped =
                addPortMapping(
                    controlUrl,
                    serviceType,
                    internalPort,
                    internalPort,
                    localIp,
                )
            if (!mapped) {
                Log.w(TAG, "AddPortMapping failed")
                return null
            }

            Log.i(TAG, "UPnP mapped $localIp:$internalPort -> $externalIp:$internalPort")
            UpnpMapping(externalIp, internalPort, internalPort, controlUrl, serviceType)
        } catch (e: Exception) {
            Log.w(TAG, "UPnP mapping failed: ${e.message}")
            null
        }
    }

    /** Remove a previously created port mapping. Best-effort, failures are logged. */
    fun removeMapping(mapping: UpnpMapping) {
        try {
            deletePortMapping(mapping.controlUrl, mapping.serviceType, mapping.externalPort)
            Log.i(TAG, "UPnP mapping removed for port ${mapping.externalPort}")
        } catch (e: Exception) {
            Log.w(TAG, "Failed to remove UPnP mapping: ${e.message}")
        }
    }

    /**
     * Send SSDP M-SEARCH and return the Location header from the first
     * IGD response, or null if no gateway responds.
     */
    private fun discoverGateway(): String? {
        val searchTarget = "urn:schemas-upnp-org:device:InternetGatewayDevice:1"
        val request =
            "M-SEARCH * HTTP/1.1\r\n" +
                "HOST: $SSDP_ADDR:$SSDP_PORT\r\n" +
                "MAN: \"ssdp:discover\"\r\n" +
                "MX: 2\r\n" +
                "ST: $searchTarget\r\n" +
                "\r\n"

        val socket = DatagramSocket()
        socket.soTimeout = SSDP_TIMEOUT_MS
        try {
            val dest = InetSocketAddress(InetAddress.getByName(SSDP_ADDR), SSDP_PORT)
            val data = request.toByteArray(Charsets.US_ASCII)
            socket.send(DatagramPacket(data, data.size, dest))

            val buf = ByteArray(2048)
            val recv = DatagramPacket(buf, buf.size)
            // Try a few receives in case we get non-IGD responses first
            for (attempt in 0 until 3) {
                try {
                    socket.receive(recv)
                    val response = String(recv.data, 0, recv.length, Charsets.US_ASCII)
                    val location = extractHeader(response, "LOCATION")
                    if (location != null) {
                        return location
                    }
                } catch (_: java.net.SocketTimeoutException) {
                    break
                }
            }
            return null
        } finally {
            socket.close()
        }
    }

    /**
     * Fetch the IGD device description XML at [location] and extract the
     * WANIPConnection or WANPPPConnection control URL.
     */
    private fun parseDeviceDescription(location: String): Pair<String, String>? {
        val xml = httpGet(location) ?: return null
        val baseUrl = extractBaseUrl(location)

        // Look for WANIPConnection first, then WANPPPConnection
        for (serviceType in listOf(
            "urn:schemas-upnp-org:service:WANIPConnection:1",
            "urn:schemas-upnp-org:service:WANPPPConnection:1",
        )) {
            val controlUrl = extractControlUrl(xml, serviceType, baseUrl)
            if (controlUrl != null) {
                return Pair(controlUrl, serviceType)
            }
        }
        return null
    }

    /**
     * Extract a control URL for the given service type from device description XML.
     * Simple text-based parsing -- avoids pulling in an XML parser dependency.
     */
    private fun extractControlUrl(
        xml: String,
        serviceType: String,
        baseUrl: String,
    ): String? {
        // Find the <service> block containing this serviceType
        val stIdx = xml.indexOf(serviceType)
        if (stIdx < 0) return null

        // Look for <controlURL> within the same <service> block
        val serviceEnd = xml.indexOf("</service>", stIdx)
        val block = if (serviceEnd > stIdx) xml.substring(stIdx, serviceEnd) else xml.substring(stIdx)

        val ctrlStart = block.indexOf("<controlURL>")
        if (ctrlStart < 0) return null
        val ctrlEnd = block.indexOf("</controlURL>", ctrlStart)
        if (ctrlEnd < 0) return null

        val path = block.substring(ctrlStart + "<controlURL>".length, ctrlEnd).trim()
        return if (path.startsWith("http")) path else "$baseUrl$path"
    }

    /** SOAP GetExternalIPAddress */
    private fun getExternalIp(
        controlUrl: String,
        serviceType: String,
    ): String? {
        val body =
            """<?xml version="1.0"?>
            |<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
            | s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
            |<s:Body>
            |<u:GetExternalIPAddress xmlns:u="$serviceType"/>
            |</s:Body>
            |</s:Envelope>
            """.trimMargin()

        val response =
            soapRequest(
                controlUrl,
                "$serviceType#GetExternalIPAddress",
                body,
            ) ?: return null

        return extractXmlValue(response, "NewExternalIPAddress")
    }

    /** SOAP AddPortMapping for UDP */
    private fun addPortMapping(
        controlUrl: String,
        serviceType: String,
        externalPort: Int,
        internalPort: Int,
        internalClient: String,
    ): Boolean {
        val body =
            """<?xml version="1.0"?>
            |<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
            | s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
            |<s:Body>
            |<u:AddPortMapping xmlns:u="$serviceType">
            |<NewRemoteHost></NewRemoteHost>
            |<NewExternalPort>$externalPort</NewExternalPort>
            |<NewProtocol>UDP</NewProtocol>
            |<NewInternalPort>$internalPort</NewInternalPort>
            |<NewInternalClient>$internalClient</NewInternalClient>
            |<NewEnabled>1</NewEnabled>
            |<NewPortMappingDescription>dxx-redux</NewPortMappingDescription>
            |<NewLeaseDuration>$UPNP_LEASE_SECONDS</NewLeaseDuration>
            |</u:AddPortMapping>
            |</s:Body>
            |</s:Envelope>
            """.trimMargin()

        val response =
            soapRequest(
                controlUrl,
                "$serviceType#AddPortMapping",
                body,
            )
        // A successful AddPortMapping returns 200 OK
        return response != null
    }

    /** SOAP DeletePortMapping */
    private fun deletePortMapping(
        controlUrl: String,
        serviceType: String,
        externalPort: Int,
    ) {
        val body =
            """<?xml version="1.0"?>
            |<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
            | s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
            |<s:Body>
            |<u:DeletePortMapping xmlns:u="$serviceType">
            |<NewRemoteHost></NewRemoteHost>
            |<NewExternalPort>$externalPort</NewExternalPort>
            |<NewProtocol>UDP</NewProtocol>
            |</u:DeletePortMapping>
            |</s:Body>
            |</s:Envelope>
            """.trimMargin()

        soapRequest(controlUrl, "$serviceType#DeletePortMapping", body)
    }

    /** Send a SOAP POST request and return the response body, or null on error. */
    private fun soapRequest(
        url: String,
        soapAction: String,
        body: String,
    ): String? {
        val conn = URL(url).openConnection() as HttpURLConnection
        conn.connectTimeout = HTTP_TIMEOUT_MS
        conn.readTimeout = HTTP_TIMEOUT_MS
        conn.requestMethod = "POST"
        conn.setRequestProperty("Content-Type", "text/xml; charset=\"utf-8\"")
        conn.setRequestProperty("SOAPAction", "\"$soapAction\"")
        conn.doOutput = true
        try {
            conn.outputStream.use { it.write(body.toByteArray(Charsets.UTF_8)) }
            val code = conn.responseCode
            if (code !in 200..299) {
                Log.w(TAG, "SOAP $soapAction returned HTTP $code")
                return null
            }
            return BufferedReader(InputStreamReader(conn.inputStream, Charsets.UTF_8))
                .use { it.readText() }
        } catch (e: Exception) {
            Log.w(TAG, "SOAP request failed: ${e.message}")
            return null
        } finally {
            conn.disconnect()
        }
    }

    /** Simple HTTP GET returning the response body, or null on error. */
    private fun httpGet(url: String): String? {
        val conn = URL(url).openConnection() as HttpURLConnection
        conn.connectTimeout = HTTP_TIMEOUT_MS
        conn.readTimeout = HTTP_TIMEOUT_MS
        try {
            val code = conn.responseCode
            if (code !in 200..299) return null
            return BufferedReader(InputStreamReader(conn.inputStream, Charsets.UTF_8))
                .use { it.readText() }
        } catch (e: Exception) {
            Log.w(TAG, "HTTP GET failed: ${e.message}")
            return null
        } finally {
            conn.disconnect()
        }
    }

    /** Extract a header value from an HTTP response string (case-insensitive). */
    private fun extractHeader(
        response: String,
        name: String,
    ): String? {
        for (line in response.split("\r\n")) {
            if (line.startsWith("$name:", ignoreCase = true)) {
                return line.substringAfter(":").trim()
            }
        }
        return null
    }

    /** Extract the base URL (scheme + host + port) from a full URL. */
    private fun extractBaseUrl(url: String): String {
        val parsed = URL(url)
        val port = if (parsed.port == -1) "" else ":${parsed.port}"
        return "${parsed.protocol}://${parsed.host}$port"
    }

    /** Extract a simple XML element value like <Tag>value</Tag>. */
    private fun extractXmlValue(
        xml: String,
        tag: String,
    ): String? {
        val start = xml.indexOf("<$tag>")
        if (start < 0) return null
        val end = xml.indexOf("</$tag>", start)
        if (end < 0) return null
        return xml.substring(start + tag.length + 2, end).trim()
    }
}
