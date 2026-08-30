# Host lobby chat layout fit

## Goal

Keep the common host lobby with the host and one client within the available screen height, without requiring the outer view to scroll, while retaining usable overflow behavior for larger lobbies.

## Plan

1. [Completed] Compare the host and client lobby layout constraints around the player list, chat area, and action controls.
2. [Completed] Change the host lobby sizing so the common two-player case fits without an outer scrollbar while chat messages retain their own scrolling.
3. [Completed] Run a scoped Kotlin quality check and compile the debug app.
