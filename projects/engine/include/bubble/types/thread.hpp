#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <thread>

namespace bubble
{
namespace chrono = std::chrono;
using std::atomic;
using std::lock_guard;
using std::mutex;
using std::shared_lock;
using std::shared_mutex;
using std::thread;
namespace this_thread = std::this_thread;
}
