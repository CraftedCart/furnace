/**
 * Furnace Tracker - multi-system chiptune tracker
 * Copyright (C) 2021-2022 tildearrow and contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _GUI_TASK_QUEUE_H
#define _GUI_TASK_QUEUE_H

#include <deque>
#include <future>
#include <memory>

/**
 * @brief Queues up tasks to be run on another thread
 *
 * Tasks can be `enqueue`d onto the `TaskQueue`, which can be executed by another thread at some point in time. A
 * `std::future` is returned from `enqueue` so the enqueuing thread can, for example, block waiting for the tasks's
 * return value.
 */
class TaskQueue {
  private:
    /**
     * @brief Non-templated base class for `Task`
     */
    struct TaskBase {
      virtual ~TaskBase() = default;
      virtual void run() = 0;
    };

    template<typename R>
    struct Task : public TaskBase {
      std::packaged_task<R()> task;

      Task(std::packaged_task<R()>&& task) noexcept :
        task(std::forward<std::packaged_task<R()>>(task)) {}

      virtual void run() override {
        task();
      }
    };

  private:
    /**
     * @brief Task queue
     *
     * New tasks are put on the back of the queue, and tasks are popped off the front when executed.
     */
    std::deque<std::unique_ptr<TaskBase>> tasks;

    /**
     * @brief Mutex protecting `tasks`
     *
     * Lock this before reading/writing from `tasks`
     */
    std::mutex tasksMutex;

  public:
    /**
     * @brief Enqueue a task to be run at a later point
     */
    template<typename R, typename F>
    std::future<R> enqueue(F&& func) {
      // Move the task onto the heap, and wrap it up in a `Task`
      std::unique_ptr<Task<R>> task = std::make_unique<Task<R>>(std::packaged_task<R()>(std::forward<F>(func)));

      std::future<R> future = task->task.get_future();

      {
        std::lock_guard<std::mutex> lock(tasksMutex);
        tasks.push_back(std::move(task));
      }

      return future;
    }

    /**
     * @brief Runs all queued tasks on the current thread
     */
    void processTasks();
};

#endif
