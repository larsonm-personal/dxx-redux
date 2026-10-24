package com.dxxredux.app.multiplayer

import android.app.Activity
import android.content.MutableContextWrapper
import android.util.Log
import androidx.credentials.ClearCredentialStateRequest
import androidx.credentials.CredentialManager
import androidx.credentials.CustomCredential
import androidx.credentials.GetCredentialRequest
import androidx.credentials.exceptions.ClearCredentialException
import androidx.credentials.exceptions.GetCredentialCancellationException
import androidx.credentials.exceptions.GetCredentialException
import androidx.credentials.exceptions.GetCredentialInterruptedException
import androidx.credentials.exceptions.GetCredentialProviderConfigurationException
import androidx.credentials.exceptions.GetCredentialUnsupportedException
import androidx.credentials.exceptions.NoCredentialException
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import com.dxxredux.app.BuildConfig
import com.google.android.libraries.identity.googleid.GetSignInWithGoogleOption
import com.google.android.libraries.identity.googleid.GoogleIdTokenCredential
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/** Shared Play/GitHub provider boundary: call only from an explicit foreground Connect action */
object GoogleMatchmakingAuth {
    val isConfigured: Boolean
        get() = BuildConfig.MATCHMAKING_GOOGLE_CLIENT_ID.let { it.isNotBlank() && !it.startsWith("YOUR_") }

    suspend fun signIn(
        activity: Activity,
        nonce: String,
    ): String =
        withContext(Dispatchers.Main.immediate) {
            check(isConfigured && !activity.isFinishing && !activity.isDestroyed)
            require(nonce.length == 64 && nonce.all { it in '0'..'9' || it in 'a'..'f' })
            val option =
                GetSignInWithGoogleOption
                    .Builder(BuildConfig.MATCHMAKING_GOOGLE_CLIENT_ID)
                    .setNonce(nonce)
                    .build()
            val request = GetCredentialRequest.Builder().addCredentialOption(option).build()
            val manager = CredentialManager.create(activity.applicationContext)
            // Reset account preference when possible; failure must not prevent first-time sign-in
            // The explicit button flow still requests a credential; reconnect never calls here
            try {
                manager.clearCredentialState(ClearCredentialStateRequest())
            } catch (_: ClearCredentialException) {
                Log.w("DXX-GoogleAuth", "Account preference reset unavailable; continuing explicit sign-in")
            }
            check((activity as? LifecycleOwner)?.lifecycle?.currentState?.isAtLeast(Lifecycle.State.RESUMED) == true)
            val result =
                try {
                    manager.getCredential(MutableContextWrapper(activity), request)
                } catch (error: GetCredentialException) {
                    providerFailure(
                        when (error) {
                            is NoCredentialException -> GoogleProviderFailure.NO_CREDENTIAL

                            is GetCredentialCancellationException -> GoogleProviderFailure.CANCELLED

                            is GetCredentialInterruptedException -> GoogleProviderFailure.INTERRUPTED

                            is GetCredentialProviderConfigurationException,
                            is GetCredentialUnsupportedException,
                            -> GoogleProviderFailure.UNAVAILABLE

                            else -> GoogleProviderFailure.UNKNOWN
                        },
                    )
                }
            val credential = result.credential as? CustomCredential ?: error("Unexpected sign-in credential")
            check(credential.type == GoogleIdTokenCredential.TYPE_GOOGLE_ID_TOKEN_CREDENTIAL)
            GoogleIdTokenCredential.createFrom(credential.data).idToken.also {
                require(it.length in 1..8192)
            }
        }

    private fun providerFailure(failure: GoogleProviderFailure): Nothing {
        // Only fixed categories: provider messages and credential objects can contain personal data
        Log.w("DXX-GoogleAuth", "Sign-in failed: ${failure.name}")
        throw GoogleProviderException(failure)
    }

    suspend fun clearAccountSelection(activity: Activity) {
        CredentialManager.create(activity.applicationContext).clearCredentialState(ClearCredentialStateRequest())
    }
}
