#include "android/android_performance_hint.h"
#include "android/g925_hint_thread_policy.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <dirent.h>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>

#include <android/log.h>

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(android_performance_hint, true, "GTA IV/Performance",
                    "Report frame times to Android's Performance Hint API so the system "
                    "raises the CPU clocks of the game's busiest threads when frames run late");
REXCVAR_DEFINE_UINT32(android_performance_hint_target_ms, 25, "GTA IV/Performance",
                      "Frame time the performance hint asks Android for. Below the 33 ms of "
                      "30 FPS so the clocks stay up between busy moments instead of dropping "
                      "and recovering (steadier, a little warmer); 33 = the old behaviour")
    .range(10, 50);

// Legacy Mali fork delivered these updates in 100ms bursts. The optional
// lower cadence improves governor response at the cost of worker wakeups.
REXCVAR_DEFINE_UINT32(android_performance_hint_report_interval_ms, 25, "GTA IV/Performance",
                      "CPU work feedback frequency (25ms responsive, 100ms old behavior); "
                      "Android remains in charge of clocks and thread scheduling")
    .range(16, 100);
REXCVAR_DEFINE_BOOL(android_performance_hint_stable_threads, true, "GTA IV/Performance",
                    "Keep important game/recorder threads in the hint session over "
                    "brief idle periods rather than switching at every 1s sample");

namespace gta4::android {
namespace {

constexpr int64_t kMaxFrameNs = 4 * 33'333'333;  // clamp for pauses and loading stalls

int64_t TargetFrameNs() {
  return int64_t(std::clamp<uint32_t>(REXCVAR_GET(android_performance_hint_target_ms), 10, 50)) * 1'000'000;
}
constexpr size_t kMaxHintThreads = 6;
// CPU ticks (100 per second) a thread needs in one second to count as busy.
constexpr uint64_t kBusyTicks = 10;

// Fixed-size frame recorder: unlike the original vector swap, the hot game
// thread never allocates memory to report a completed frame.
std::mutex g_frames_mutex;
std::array<int64_t, 256> g_frame_durations{};
size_t g_frame_count = 0;
int64_t g_last_frame_ns = 0;

int64_t NowNs() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return int64_t(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

// NDK API (libandroid.so, API 33+; setThreads is API 34+), loaded at runtime
// because the app's minSdk is lower.
struct HintApi {
  using GetManagerFn = void* (*)();
  using CreateSessionFn = void* (*)(void*, const int32_t*, size_t, int64_t);
  using ReportFn = int (*)(void*, int64_t);
  using SetThreadsFn = int (*)(void*, const pid_t*, size_t);
  using CloseFn = void (*)(void*);
  using UpdateTargetFn = int (*)(void*, int64_t);
  using PreferPowerFn = int (*)(void*, bool);
  GetManagerFn get_manager = nullptr;
  CreateSessionFn create_session = nullptr;
  ReportFn report = nullptr;
  SetThreadsFn set_threads = nullptr;
  CloseFn close = nullptr;
  UpdateTargetFn update_target = nullptr;
  PreferPowerFn prefer_power = nullptr;

  bool Load() {
    void* library = dlopen("libandroid.so", RTLD_NOW);
    if (!library) return false;
    get_manager = reinterpret_cast<GetManagerFn>(dlsym(library, "APerformanceHint_getManager"));
    create_session =
        reinterpret_cast<CreateSessionFn>(dlsym(library, "APerformanceHint_createSession"));
    report =
        reinterpret_cast<ReportFn>(dlsym(library, "APerformanceHint_reportActualWorkDuration"));
    set_threads = reinterpret_cast<SetThreadsFn>(dlsym(library, "APerformanceHint_setThreads"));
    close = reinterpret_cast<CloseFn>(dlsym(library, "APerformanceHint_closeSession"));
    update_target = reinterpret_cast<UpdateTargetFn>(
        dlsym(library, "APerformanceHint_updateTargetWorkDuration"));
    prefer_power = reinterpret_cast<PreferPowerFn>(
        dlsym(library, "APerformanceHint_setPreferPowerEfficiency"));
    return get_manager && create_session && report && close;
  }
};

// The busiest threads of this process over the last second, busiest first.
class BusyThreads {
 public:
  std::vector<pid_t> Sample(pid_t exclude) {
    std::unordered_map<pid_t, uint64_t> ticks;
    std::vector<std::pair<uint64_t, pid_t>> busy;
    std::vector<std::pair<uint64_t, pid_t>> critical;
    if (DIR* tasks = opendir("/proc/self/task")) {
      while (const dirent* entry = readdir(tasks)) {
        if (!std::isdigit(static_cast<unsigned char>(entry->d_name[0]))) continue;
        const pid_t tid = pid_t(std::atoi(entry->d_name));
        if (tid == exclude) continue;
        std::ifstream stat_file(std::string("/proc/self/task/") + entry->d_name + "/stat");
        std::string stat((std::istreambuf_iterator<char>(stat_file)), {});
        const size_t name_end = stat.rfind(')');
        if (name_end == std::string::npos) continue;
        // Fields after the name start at 3 (state); utime is 14, stime 15.
        std::istringstream fields(stat.substr(name_end + 2));
        std::string field;
        uint64_t utime = 0, stime = 0;
        for (int index = 3; index <= 15 && fields >> field; ++index) {
          if (index == 14) utime = std::strtoull(field.c_str(), nullptr, 10);
          if (index == 15) stime = std::strtoull(field.c_str(), nullptr, 10);
        }
        ticks[tid] = utime + stime;
        const auto last = last_ticks_.find(tid);
        const uint64_t delta = last != last_ticks_.end() &&
                               utime + stime >= last->second
            ? utime + stime - last->second : 0;
        // Already loaded /proc stat contains truncated Linux thread names.
        // Protect frame-critical roles even when renderer waits hide CPU use.
        const size_t begin = stat.find('(');
        const std::string name = begin < name_end
            ? stat.substr(begin + 1, name_end - begin - 1) : std::string{};
        if (name.rfind("Main XThread", 0) == 0 ||
            name.rfind("GtaRecorder", 0) == 0) {
          critical.emplace_back(delta, tid);
        } else if (delta >= kBusyTicks) {
          busy.emplace_back(delta, tid);
        }
      }
      closedir(tasks);
    }
    last_ticks_ = std::move(ticks);
    std::sort(critical.begin(), critical.end(), std::greater<>());
    std::sort(busy.begin(), busy.end(), std::greater<>());
    std::vector<pid_t> result;
    for (const auto& [delta, tid] : critical) {
      if (result.size() == kMaxHintThreads) break;
      result.push_back(tid);
    }
    for (const auto& [delta, tid] : busy) {
      if (result.size() == kMaxHintThreads) break;
      result.push_back(tid);
    }
    std::sort(result.begin(), result.end());
    return result;
  }

 private:
  std::unordered_map<pid_t, uint64_t> last_ticks_;
};

std::string ThreadName(pid_t tid) {
  std::ifstream comm("/proc/self/task/" + std::to_string(tid) + "/comm");
  std::string name;
  std::getline(comm, name);
  return name.empty() ? "?" : name;
}

void LogThreads(const char* what, const std::vector<pid_t>& threads) {
  std::string list;
  for (pid_t tid : threads) {
    if (!list.empty()) list += ", ";
    list += ThreadName(tid) + ":" + std::to_string(tid);
  }
  __android_log_print(ANDROID_LOG_WARN, "LibertyRecomp", "performance-hint: %s %zu threads [%s]", what,
                      threads.size(), list.c_str());
}

void Log(const char* message, size_t count) {
  __android_log_print(ANDROID_LOG_WARN, "LibertyRecomp", "performance-hint: %s (%zu threads)",
                      message, count);
}

}  // namespace

void NoteTitleFrame() noexcept {
  if (!REXCVAR_GET(android_performance_hint)) return;
  const int64_t now = NowNs();
  std::lock_guard lock(g_frames_mutex);
  if (g_last_frame_ns != 0 && now > g_last_frame_ns) {
    // Clamp pauses and loading stalls so one long gap does not dominate.
    const int64_t duration = std::min<int64_t>(now - g_last_frame_ns, kMaxFrameNs);
    if (g_frame_count < g_frame_durations.size()) {
      g_frame_durations[g_frame_count++] = duration;
    } else {
      // Reporter may fall behind during load; retain newest bounded sample.
      g_frame_durations.back() = duration;
    }
  }
  g_last_frame_ns = now;
}

void StartPerformanceHint() {
  std::thread([] {
    HintApi api;
    if (!api.Load()) {
      Log("not available on this Android version", 0);
      return;
    }
    void* manager = api.get_manager();
    if (!manager) {
      Log("no hint manager", 0);
      return;
    }
    const pid_t self = pid_t(gettid());
    BusyThreads busy;
    G925StableHintThreads<pid_t> stable_threads{kMaxHintThreads};
    bool was_stable = REXCVAR_GET(android_performance_hint_stable_threads);
    std::vector<pid_t> threads;
    void* session = nullptr;
    int64_t session_target = 0;
    bool sampled_baseline = false;
    auto last_thread_scan = std::chrono::steady_clock::now();
    auto last_config_report = last_thread_scan - std::chrono::seconds(10);
    for (;;) {
      const uint32_t interval = std::clamp<uint32_t>(
          REXCVAR_GET(android_performance_hint_report_interval_ms), 16, 100);
      std::this_thread::sleep_for(std::chrono::milliseconds(interval));
      const auto tick = std::chrono::steady_clock::now();
      if (tick - last_config_report >= std::chrono::seconds(10)) {
        last_config_report = tick;
        __android_log_print(
            ANDROID_LOG_WARN, "LibertyRecomp",
            "performance-hint: config enabled=%d stable=%d target-ms=%lld "
            "report-ms=%u session=%d members=%zu",
            int(REXCVAR_GET(android_performance_hint)),
            int(REXCVAR_GET(android_performance_hint_stable_threads)),
            static_cast<long long>(TargetFrameNs() / 1000000), interval,
            int(session != nullptr), threads.size());
      }
      if (!REXCVAR_GET(android_performance_hint)) {
        if (session) {
          api.close(session);
          session = nullptr;
          threads.clear();
          Log("turned off", 0);
        }
        sampled_baseline = false;
        stable_threads.Reset();
        threads.clear();
        // Avoid a 256-entry vector shifting on every title frame when ADPF
        // is disabled. No frame durations should accumulate in that mode.
        {
          std::lock_guard lock(g_frames_mutex);
          g_frame_count = 0;
          g_last_frame_ns = 0;
        }
        continue;
      }
      const auto scan_now = std::chrono::steady_clock::now();
      if (!sampled_baseline) {
        busy.Sample(self);  // Establish initial CPU tick baseline.
        sampled_baseline = true;
        last_thread_scan = scan_now;
      }
      // Scan active threads once a second, independent of report interval.
      if (scan_now - last_thread_scan >= std::chrono::seconds(1)) {
        last_thread_scan = scan_now;
        std::vector<pid_t> hot = busy.Sample(self);
        // The 2.1 G925 report shows near-constant six-thread swaps.
        // Hysteresis keeps living frame-critical threads across two misses.
        const bool steady = REXCVAR_GET(android_performance_hint_stable_threads);
        if (steady != was_stable) {
          stable_threads.Reset();
          was_stable = steady;
        }
        if (steady) {
          const auto& selected = stable_threads.Select(hot, [](pid_t tid) {
            return access(("/proc/self/task/" + std::to_string(tid)).c_str(), F_OK) == 0;
          });
          hot.assign(selected.begin(), selected.end());
        }
        if (!hot.empty() && hot != threads) {
          if (session && api.set_threads &&
              api.set_threads(session, hot.data(), hot.size()) == 0) {
            threads = hot;
            LogThreads(steady ? "stable threads updated" : "now boosting", threads);
          } else {
            if (session) api.close(session);
            session = api.create_session(manager, reinterpret_cast<const int32_t*>(hot.data()),
                                         hot.size(), TargetFrameNs());
            session_target = TargetFrameNs();
            threads = session ? hot : std::vector<pid_t>{};
            if (session) {
              // Android 15+: ask for performance over power efficiency.
              if (api.prefer_power) api.prefer_power(session, false);
              LogThreads(steady ? "stable threads started," : "session started,", threads);
            } else {
              Log("could not create a session", hot.size());
            }
          }
        }
      }
      // Follow a changed target (live tuning) without recreating the session.
      if (session && api.update_target) {
        const int64_t target = TargetFrameNs();
        if (target != session_target && api.update_target(session, target) == 0) {
          session_target = target;
        }
      }
      // No heap allocation in the reporting path; only copy valid samples.
      std::array<int64_t, 256> durations;
      size_t duration_count = 0;
      {
        std::lock_guard lock(g_frames_mutex);
        duration_count = g_frame_count;
        std::copy_n(g_frame_durations.begin(), duration_count, durations.begin());
        g_frame_count = 0;
      }
      if (session) {
        for (size_t i = 0; i < duration_count; ++i) {
          if (durations[i] > 0) api.report(session, durations[i]);
        }
      }
    }
  }).detach();
}

}  // namespace gta4::android
