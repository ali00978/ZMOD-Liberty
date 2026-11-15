# G925 2.12: overlap native rendering with presentation admission

Version code 40, `0.5.9.3-mali-alpha-2.12-g925-present-overlap`.

## Evidence and cause

The user's valid 2.11 capture (2026-11-03T22:38:10.847Z) reports mean GPU time
12.31 ms, command recording 18.48 ms, callback 24.49 ms, and title interval
36.95 ms. CPU detail covers 179/180 frames; `refresh-output` has 10.436 ms
exclusive elapsed time. GPU fence and pipeline waits are zero in this sample.
These spans overlap and profiling adds overhead; they are not a normal FPS test.

The native worker opts into presenter backpressure. Previously each publication
waited for its **own** UI/presenter admission before the worker could start the
next scene. UI scheduling and presentation pacing could therefore be added to
rendering instead of overlapping it. The broad refresh-output measurement
includes this wait; it does not prove that all 10.436 ms came from the gate.
CPU recording also remains above the 16.67 ms budget for 60 FPS in this capture.

## Change

Android native rendering opts into a bounded publication pipeline. After one
frame is published, the worker may render and submit the next writable mailbox
image. It then waits for the prior publication **before** publishing/rotating
the new image. There is still at most one unadmitted ready publication, and
the existing two native GPU frame slots and three mailbox images remain intact.

The wait occurs after the backend callback releases its image access lock and
before any presenter/queue/consumer lock is taken. Actual GPU completion fences,
shared-image semaphore tickets, allocation lifetimes, presentation cap and
swapchain synchronization are unchanged. This permits one frame of CPU lead;
it does not permit an unbounded queue. It can trade a bounded amount of input
latency for throughput.

Generated/real frame pairs retain their existing immediate admission wait.
Other renderers/platforms retain the default behavior. UI-thread refreshes
never wait on themselves. Surface loss, shutdown, unlimited mode and the
existing 250 ms missing-callback watchdog still release blocked workers.
Failed refreshes keep the pending prior publication for the next successful
publication to drain.

A synchronous optional wait observer reports `presenter-publication-wait` as
a child of `refresh-output`, separating sleep/admission time from other work.
Graphics, assets and CPU hint settings are unchanged.

## Validation and delivery

The actual gate and production pipeline pass a two-thread test proving that
the second frame can render before acknowledgement, but cannot publish early.
Tests cover mode changes, generated pairs, UI refreshes, inactive/uncapped paths,
surface/shutdown releases, 1,000-frame bounded timelines, unchanged 30/40/60/120
presentation caps and exclusive wait accounting. The suite passes normally
and under AddressSanitizer/UndefinedBehaviorSanitizer. Java capture/report and
the existing 16 ms and batched-CPU policy tests pass; `git diff --check` passes.

Full native Android compilation and APK packaging run in GitHub Actions.
The build link is handed off immediately so the user can monitor completion.
No device-side speedup or guaranteed 60 FPS is claimed before device validation.

Previous build: [2.11](https://github.com/ali00978/ZMOD-Liberty/actions/runs/41721965132).
