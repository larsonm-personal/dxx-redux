package com.dxxredux.app

internal class AudioPreviewResourceOwner<T : Any>(
    private val releaseResource: (T) -> Unit,
) {
    var current: T? = null
        private set

    fun replace(resource: T): T {
        val previous = current
        current = resource
        if (previous !== resource) previous?.let(releaseResource)
        return resource
    }

    fun release(expected: T? = current): Boolean {
        if (expected == null || current !== expected) return false
        current = null
        releaseResource(expected)
        return true
    }
}

internal fun <T : Any> initializeAudioPreviewResource(
    owner: AudioPreviewResourceOwner<T>,
    resource: T,
    initialize: (T) -> Unit,
): T {
    owner.replace(resource)
    try {
        initialize(resource)
        return resource
    } catch (failure: Throwable) {
        owner.release(resource)
        throw failure
    }
}
