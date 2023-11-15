/**
 * Copyright (c) 2021 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#pragma once

#include <deque>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace oceanbase {
namespace logproxy {
template <typename T>
class BlockingQueue {
public:
  explicit BlockingQueue(const size_t max_queue_size = S_DEFAULT_MAX_QUEUE_SIZE) : _max_queue_size(max_queue_size)
  {
    // nothing to do
  }

  bool offer(const T& element, uint64_t timeout_us)
  {
    std::unique_lock<std::mutex> op_lock(_op_mutex);
    while (_queue.size() >= _max_queue_size) {
      std::cv_status st = _not_full.wait_for(op_lock, std::chrono::microseconds(timeout_us));
      if (st == std::cv_status::timeout) {
        return false;
      }
    }

    if (_queue.size() >= _max_queue_size) {
      return false;
    }
    _queue.emplace_back(element);
    _not_empty.notify_one();
    return true;
  }

  bool poll(T& element, uint64_t timeout_us)
  {
    std::unique_lock<std::mutex> op_lock(_op_mutex);
    while (_queue.empty()) {
      std::cv_status st = _not_empty.wait_for(op_lock, std::chrono::microseconds(timeout_us));
      if (st == std::cv_status::timeout) {
        return false;
      }
    }

    if (_queue.empty()) {
      return false;
    }
    element = _queue.front();
    _queue.pop_front();
    _not_full.notify_one();
    return true;
  }

  bool poll(std::vector<T>& elements, uint64_t timeout_us)
  {
    std::unique_lock<std::mutex> op_lock(_op_mutex);
    while (_queue.empty()) {
      std::cv_status st = _not_empty.wait_for(op_lock, std::chrono::microseconds(timeout_us));
      if (st == std::cv_status::timeout) {
        return false;
      }
    }
    if (_queue.empty()) {
      return false;
    }

    while (!_queue.empty() && elements.size() < elements.capacity()) {
      elements.push_back(_queue.front());
      _queue.pop_front();
    }

    _not_full.notify_one();
    return true;
  }

  size_t size(bool safe = true)
  {
    if (safe) {
      std::lock_guard<std::mutex> lock(_op_mutex);
      return _queue.size();
    } else {
      return _queue.size();
    }
  }

  void clear()
  {
    clear([] {});
  }

  void clear(std::function<void(T&)> foreach)
  {
    std::lock_guard<std::mutex> lock(_op_mutex);
    while (!_queue.empty()) {
      if (foreach) {
        foreach (_queue.front())
          ;
      }
      _queue.pop_front();
    }
  }

private:
  static const size_t S_DEFAULT_MAX_QUEUE_SIZE = 60000;

  size_t _max_queue_size;
  std::deque<T> _queue;
  std::mutex _op_mutex;
  std::condition_variable _not_empty;
  std::condition_variable _not_full;
};

}  // namespace logproxy
}  // namespace oceanbase
