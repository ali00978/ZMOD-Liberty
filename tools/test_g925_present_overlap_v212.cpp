#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <rex/ui/frame_pacer.h>
#include "graphics/gta4_native/native_cpu_profile.h"

using namespace rex::ui;
using namespace std::chrono_literals;
static void check(bool ok) { if (!ok) throw std::runtime_error("publication overlap regression"); }

static void concurrent_publication() {
  FramePublicationGate gate;
  FramePublicationPipeline pipeline;
  gate.SetAvailable(true);
  std::promise<uint64_t> first, second;
  auto first_result = first.get_future(), second_result = second.get_future();
  std::promise<void> rendering_next, attempting_publication;
  auto rendering = rendering_next.get_future(), attempting = attempting_publication.get_future();
  auto worker = std::async(std::launch::async, [&] {
    auto wait = [&](uint64_t serial) { check(gate.Wait(serial)); };
    pipeline.BeforePublish(false, wait);
    const auto a = gate.Publish(60);
    first.set_value(a);
    pipeline.AfterPublish(a, true, true, false, wait);
    rendering_next.set_value(); // Next callback is reachable before UI acknowledgement.
    attempting_publication.set_value();
    pipeline.BeforePublish(false, wait);
    const auto b = gate.Publish(60);
    second.set_value(b);
    pipeline.AfterPublish(b, true, true, false, wait);
    pipeline.BeforePublish(false, wait);
  });
  const auto a = first_result.get();
  check(rendering.wait_for(100ms) == std::future_status::ready);
  check(attempting.wait_for(100ms) == std::future_status::ready);
  check(second_result.wait_for(2ms) == std::future_status::timeout);
  gate.Accept(a);
  const auto b = second_result.get();
  check(b == a + 1);
  gate.Accept(b);
  worker.get();
}

static void modes() {
  FramePublicationPipeline pipeline;
  std::vector<uint64_t> waited;
  auto wait = [&](uint64_t serial) { waited.push_back(serial); };
  pipeline.AfterPublish(1, true, true, false, wait);
  check(waited.empty());
  // A failed refresh does not reach BeforePublish: keep the prior serial.
  pipeline.BeforePublish(false, wait);
  check(waited == std::vector<uint64_t>{1});
  pipeline.BeforePublish(false, wait); check(waited.size() == 1);
  pipeline.AfterPublish(2, true, true, true, wait); // Generated/real pair stays strict.
  pipeline.AfterPublish(3, true, false, false, wait); // Existing non-Android path.
  check(waited == std::vector<uint64_t>({1,2,3}));
  pipeline.AfterPublish(4, false, true, false, wait); // Uncapped/inactive/GPU lost/UI.
  pipeline.BeforePublish(false, wait); check(waited.size() == 3);
  pipeline.AfterPublish(5, true, true, false, wait);
  pipeline.BeforePublish(true, wait); // UI cannot wait for itself.
  pipeline.BeforePublish(false, wait); check(waited.size() == 3);
}

static void lifecycle() {
  for (unsigned mode = 0; mode < 4; ++mode) {
    FramePublicationGate gate;
    gate.SetAvailable(true);
    const auto serial = gate.Publish(60);
    std::promise<void> entered;
    auto ready = entered.get_future();
    auto worker = std::async(std::launch::async, [&] {
      entered.set_value(); return gate.Wait(serial);
    });
    ready.get();
    if (mode == 0) gate.SetAvailable(false);
    if (mode == 1) gate.Stop();
    if (mode == 2) gate.Publish(0);
    if (mode == 3) gate.Admit(serial);
    check(worker.get());
  }
}

static uint64_t timeline(bool overlap) {
  FramePublicationPipeline pipeline;
  uint64_t cpu = 0, previous_display = 0, retired = 0;
  std::vector<uint64_t> display(1);
  for (uint64_t frame = 1; frame <= 1000; ++frame) {
    // Deliberately synthetic times, not a model of phone FPS.
    cpu += 24'000'000;
    auto wait = [&](uint64_t serial) {
      cpu = std::max(cpu, display.at(serial)); retired = serial;
    };
    pipeline.BeforePublish(false, wait);
    check(frame == 1 || retired == frame - 1); // Never replace an unadmitted image.
    previous_display = std::max(cpu + 10'000'000, previous_display + 16'666'667);
    display.push_back(previous_display);
    pipeline.AfterPublish(frame, true, overlap, false, wait);
  }
  pipeline.BeforePublish(false, [&](uint64_t serial) { cpu = std::max(cpu, display.at(serial)); });
  return cpu;
}

static void cap_and_accounting() {
  for (uint32_t fps : {30u, 40u, 60u, 120u}) {
    FramePacer pacer;
    uint64_t now = 1'000'000'000, first = now;
    pacer.Configure(fps, now);
    for (unsigned frame = 0; frame < 1000; ++frame) {
      auto plan = pacer.Plan(now);
      if (plan.delay_ns) { now += plan.delay_ns; plan = pacer.Plan(now); }
      check(!plan.delay_ns);
      check(now - first >= uint64_t(frame) * FramePacer::kSecond / fps);
      pacer.Queued(now);
      now += 100; // Fast producer still cannot evade the cap.
    }
  }
  namespace p = rex::graphics::gta4_native::profile;
  p::CpuRecorder recorder;
  check(recorder.Begin(0));
  auto refresh = recorder.Enter(p::CpuOp::kRefreshOutput, 0);
  auto wait = recorder.Enter(p::CpuOp::kPresenterWait, 5);
  check(recorder.Leave(wait, 15));
  check(recorder.Leave(refresh, 20));
  const auto result = recorder.Finish(20);
  check(result.measurements.size() == 2 && result.invalid_scopes == 0);
  for (const auto& item : result.measurements) {
    check(item.self_ticks == 10);
    if (item.op == p::CpuOp::kPresenterWait)
      check(std::string(p::CpuOpName(item.op)) == "presenter-publication-wait");
  }
}

int main() {
  concurrent_publication(); modes(); lifecycle(); cap_and_accounting();
  check(timeline(false) == 34'000'000'000);
  check(timeline(true) == 24'010'000'000);
  std::cout << "PASS: actual gate/thread overlap, bounded publication, paired/UI modes, lifecycle releases, "
               "1000-frame timelines, 30/40/60/120 caps and exclusive wait accounting\n";
}
