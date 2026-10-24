package com.dxxredux.app.multiplayer

import com.sun.net.httpserver.HttpsConfigurator
import com.sun.net.httpserver.HttpsServer
import kotlinx.coroutines.runBlocking
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeNotNull
import org.junit.Test
import java.io.IOException
import java.net.InetSocketAddress
import java.nio.file.Files
import java.security.KeyStore
import java.util.concurrent.CompletableFuture
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import javax.net.ssl.KeyManagerFactory
import javax.net.ssl.SSLContext
import javax.net.ssl.TrustManagerFactory
import javax.net.ssl.X509TrustManager

/** Real Android helper -> owned HTTPS termination -> real Rust login and signed-token verifier */
class GoogleLoginHttpsTest {
    @Test
    fun signedLoginAndRevocationAcrossHttps() =
        runBlocking {
            val binary = System.getProperty("dxx.googleFixtureBinary")
            assumeNotNull(binary)
            val directory = Files.createTempDirectory("dxx-google-https-")
            val storePath = directory.resolve("fixture.p12")
            val password = "public-fixture-password"
            val keytool =
                ProcessBuilder(
                    "${System.getProperty("java.home")}/bin/keytool",
                    "-genkeypair",
                    "-alias",
                    "fixture",
                    "-keyalg",
                    "RSA",
                    "-keysize",
                    "2048",
                    "-storetype",
                    "PKCS12",
                    "-keystore",
                    storePath.toString(),
                    "-storepass",
                    password,
                    "-dname",
                    "CN=localhost",
                    "-ext",
                    "SAN=ip:127.0.0.1,dns:localhost",
                    "-validity",
                    "2",
                ).redirectErrorStream(true).start()
            check(keytool.waitFor(20, TimeUnit.SECONDS)) { "Fixture key generation timed out" }
            check(keytool.exitValue() == 0) { keytool.inputStream.bufferedReader().readText() }
            val store = KeyStore.getInstance("PKCS12")
            Files.newInputStream(storePath).use { store.load(it, password.toCharArray()) }
            val keys = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm())
            keys.init(store, password.toCharArray())
            val trust = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm())
            trust.init(store)
            val tls = SSLContext.getInstance("TLS")
            tls.init(keys.keyManagers, trust.trustManagers, null)
            val proxy = HttpsServer.create(InetSocketAddress("127.0.0.1", 0), 0)
            proxy.httpsConfigurator = HttpsConfigurator(tls)
            val executor = Executors.newCachedThreadPool()
            proxy.executor = executor
            val origin = "https://127.0.0.1:${proxy.address.port}"
            val processBuilder =
                ProcessBuilder(
                    binary,
                    "google_identity::tests::android_https_login_fixture",
                    "--ignored",
                    "--exact",
                    "--nocapture",
                ).redirectErrorStream(true)
            processBuilder.environment()["DXX_GOOGLE_FIXTURE_ORIGIN"] = origin
            val process = processBuilder.start()
            val port = CompletableFuture<Int>()
            executor.submit {
                process.inputStream.bufferedReader().useLines { lines ->
                    lines.forEach { line ->
                        if (line.startsWith("DXX_GOOGLE_FIXTURE_PORT=")) port.complete(line.substringAfter('=').toInt())
                    }
                }
                port.completeExceptionally(IllegalStateException("Rust fixture exited before readiness"))
            }
            val backend = OkHttpClient.Builder().callTimeout(5, TimeUnit.SECONDS).build()
            val http =
                OkHttpClient
                    .Builder()
                    .sslSocketFactory(tls.socketFactory, trust.trustManagers.single() as X509TrustManager)
                    .followRedirects(false)
                    .followSslRedirects(false)
                    .callTimeout(5, TimeUnit.SECONDS)
                    .build()
            try {
                val backendPort = port.get(20, TimeUnit.SECONDS)
                proxy.createContext("/") { exchange ->
                    exchange.use {
                        val body = exchange.requestBody.readBytes()
                        val request = Request.Builder().url("http://127.0.0.1:$backendPort${exchange.requestURI}")
                        if (exchange.requestMethod ==
                            "POST"
                        ) {
                            request.post(body.toRequestBody("application/json".toMediaType()))
                        }
                        backend.newCall(request.build()).execute().use { response ->
                            val bytes = response.body.bytes()
                            exchange.sendResponseHeaders(
                                response.code,
                                if (bytes.isEmpty()) -1 else bytes.size.toLong(),
                            )
                            if (bytes.isNotEmpty()) exchange.responseBody.write(bytes)
                        }
                    }
                }
                proxy.start()
                val client = GoogleLoginClient(origin.replace("https://", "wss://") + "/ws", http)

                fun providerToken(nonce: String): String =
                    http
                        .newCall(
                            Request.Builder().url("$origin/fixture/token?nonce=$nonce").build(),
                        ).execute()
                        .use {
                            check(it.isSuccessful)
                            it.body.string()
                        }
                val session = client.signIn { providerToken(it) }

                fun active(credential: String): Boolean {
                    val body = JSONObject().put("session_token", credential).toString()
                    val request =
                        Request
                            .Builder()
                            .url("$origin/fixture/check")
                            .post(body.toRequestBody("application/json".toMediaType()))
                            .build()
                    return http.newCall(request).execute().use { JSONObject(it.body.string()).getBoolean("active") }
                }
                assertTrue(session.validFor(origin))
                assertTrue(active(session.credential))
                client.revoke(session)
                assertFalse(active(session.credential))
                val wrongNonce = runCatching { client.signIn { providerToken("wrong-recipient-nonce") } }
                assertTrue(wrongNonce.exceptionOrNull() is IllegalStateException)

                var lostCredential: String? = null
                val lossyHttp =
                    http
                        .newBuilder()
                        .addInterceptor { chain ->
                            val response = chain.proceed(chain.request())
                            if (chain.request().url.encodedPath == "/api/v1/auth/google") {
                                response.use {
                                    val body = JSONObject(it.body.string())
                                    lostCredential = body.getString("session_token")
                                }
                                throw IOException("Owned fixture drops issued response")
                            }
                            response
                        }.build()
                val lossyClient = GoogleLoginClient(origin.replace("https://", "wss://") + "/ws", lossyHttp)
                assertTrue(runCatching { lossyClient.signIn { providerToken(it) } }.exceptionOrNull() is IOException)
                assertTrue(lostCredential != null)
                assertFalse(active(requireNotNull(lostCredential)))
            } finally {
                proxy.stop(0)
                process.destroyForcibly()
                process.waitFor(5, TimeUnit.SECONDS)
                executor.shutdownNow()
                for (client in listOf(http, backend)) {
                    client.dispatcher.executorService.shutdownNow()
                    client.connectionPool.evictAll()
                }
                Files.deleteIfExists(storePath)
                Files.deleteIfExists(directory)
            }
        }
}
