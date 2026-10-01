package com.dxxredux.app.multiplayer

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import com.dxxredux.app.SetupActivity
import java.util.UUID

data class LanJoinRequest(
    val id: String,
    val address: String,
) {
    companion object {
        const val EXTRA_ADDRESS = "lan_join_address"
        const val EXTRA_ID = "lan_join_request_id"

        fun fromIntent(intent: Intent): LanJoinRequest? {
            val address = intent.getStringExtra(EXTRA_ADDRESS)?.let(LanInvitation::validAddress)
            val id = intent.getStringExtra(EXTRA_ID)
            intent.removeExtra(EXTRA_ADDRESS)
            intent.removeExtra(EXTRA_ID)
            return if (address != null && id != null && id.length <= 64) LanJoinRequest(id, address) else null
        }
    }
}

/** Validates external data before forwarding to the launcher's normal LAN flow */
class LanJoinLinkActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val address =
            if (intent.action == Intent.ACTION_VIEW) {
                intent.dataString?.let { LanInvitation.parse(it, LanHostAddresses.broadcastAddresses()) }
            } else {
                null
            }
        if (address == null) {
            Toast.makeText(this, "This is not a Descent LAN invitation", Toast.LENGTH_LONG).show()
        } else {
            startActivity(
                Intent(this, SetupActivity::class.java).apply {
                    addFlags(
                        Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_REORDER_TO_FRONT or
                            Intent.FLAG_ACTIVITY_SINGLE_TOP,
                    )
                    putExtra(LanJoinRequest.EXTRA_ADDRESS, address)
                    putExtra(LanJoinRequest.EXTRA_ID, UUID.randomUUID().toString())
                },
            )
        }
        finish()
    }
}
