#!/usr/bin/env python3
"""Verify Android notification-shade focus recovery in the actual SDL window source."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / "glue/rexglue-sdk-main/src/ui/window_sdl.cpp").read_text()
start = source.index("void WindowSDL::HandleAppLifecycleEvent(SDL_Event& event) {")
end = source.index("void WindowSDL::HandleTouchEvent(", start)
body = source[start:end]
background, foreground = body.split("    case SDL_EVENT_DID_ENTER_FOREGROUND:", 1)
# Background must continue detaching the surface, once, before releasing it.
for event in ("SDL_EVENT_WILL_ENTER_BACKGROUND", "SDL_EVENT_DID_ENTER_BACKGROUND",
              "SDL_EVENT_WINDOW_MINIMIZED", "SDL_EVENT_WINDOW_HIDDEN"):
    assert f"case {event}:" in background, event
assert "if (!surface_detached_)" in background
assert "surface_detached_ = true;" in background
assert "OnSurfaceChanged(false);" in background

# Android may only dispatch focus regained / exposed after a notification
# shade; a RESTORED event is not guaranteed. Those events must join the
# normal foreground path and not create another swapchain unnecessarily.
for event in ("SDL_EVENT_WINDOW_RESTORED", "SDL_EVENT_WINDOW_SHOWN",
              "SDL_EVENT_WINDOW_FOCUS_GAINED", "SDL_EVENT_WINDOW_EXPOSED"):
    assert f"case {event}:" in foreground, event
assert "if (surface_detached_ && GetNativeWindowHandle())" in foreground
assert "surface_detached_ = false;" in foreground
assert "OnSurfaceChanged(true);" in foreground
assert foreground.index("if (surface_detached_ && GetNativeWindowHandle())") < foreground.index(
    "OnSurfaceChanged(true);")
assert foreground.index("surface_detached_ = false;") < foreground.index(
    "OnSurfaceChanged(true);")

# A focus change with the same surface is not a GPU reset. This existing
# guard must remain; repeated notifications cannot rebuild Vulkan.
attach_block = foreground
assert attach_block.count("OnSurfaceChanged(true);") == 1
assert "surface_detached_ && GetNativeWindowHandle()" in attach_block
# Normal focus handling still forwards the event to the app and input layer.
window_handler = source[source.index("void WindowSDL::HandleWindowEvent("):
                        source.index("bool WindowSDL::GetPhysicalSafeArea(")]
assert "HandleAppLifecycleEvent(event);" in window_handler
assert "case SDL_EVENT_WINDOW_FOCUS_GAINED:" in window_handler
assert "OnFocusUpdate(true, destruction_receiver);" in window_handler
# Focus recovery must not race with the guest-output paint thread or rebuild
# the Vulkan swapchain merely to clear a stale Android display phase.
header = (root / "glue/rexglue-sdk-main/include/rex/ui/presenter.h").read_text()
presenter = (root / "glue/rexglue-sdk-main/src/ui/presenter.cpp").read_text()
focus_case = window_handler.split("case SDL_EVENT_WINDOW_FOCUS_GAINED:", 1)[1].split(
    "case SDL_EVENT_WINDOW_FOCUS_LOST:", 1)[0]
assert "active_presenter->RequestFramePacerResync();" in focus_case
assert "RequestFramePacerResync()" in header
assert "std::atomic<bool> frame_pacer_resync_requested_{false};" in header
assert "frame_pacer_resync_requested_.store(true, std::memory_order_release);" in header
paint = presenter[presenter.index("Presenter::PaintResult Presenter::PaintAndPresent("):]
assert "frame_pacer_resync_requested_.exchange(false, std::memory_order_acq_rel)" in paint
assert paint.index("frame_pacer_.Reset();") < paint.index("frame_pacer_.Configure(")
# A 500-ms timed wait already sleeps while credits are unavailable.
# A second 500-ms sleep makes the mixer unresponsive during Android resume.
audio = (root / "glue/rexglue-sdk-main/src/audio/audio_system.cpp").read_text()
begin = audio.index("void AudioSystem::WorkerThreadMain()")
end = audio.index("int AudioSystem::FindFreeClient()", begin)
worker = audio[begin:end]
assert "std::chrono::milliseconds(500)" in worker
assert "rex::thread::Sleep(std::chrono::milliseconds(500))" not in worker
timeout = worker[worker.index("if (result.first == rex::thread::WaitResult::kTimeout) {"):]
assert "continue;" in timeout.split("bool pumped = false;", 1)[0]
assert "rex::thread::Sleep(std::chrono::milliseconds(5))" in worker
print("G925 v2.27 Android notification surface/audio recovery regression: PASS")
