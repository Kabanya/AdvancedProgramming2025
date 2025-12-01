#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>


class BlockingQueue
{
public:
	using Task = std::function<void()>;

	void Push(Task task) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			buffer_.push_back(std::move(task));
		}
		not_empty_.notify_one();
	}

	Task Pop() {
		std::unique_lock<std::mutex> lock(mutex_);
		while (buffer_.empty()) {
				not_empty_.wait(lock);
		}
		Task task = std::move(buffer_.front());
		buffer_.pop_front();
		return task;
	}

private:
	std::mutex mutex_;
	std::condition_variable not_empty_;
	std::deque<Task> buffer_;
};

class ThreadPool {
public:
	using Task = std::function<void()>;

	explicit ThreadPool(size_t threads)
			: threads_(threads), stop_(false) {}

	void Start() {
			for (size_t i = 0; i < threads_; ++i) {
					workers_.emplace_back([this] {
							WorkerRoutine();
					});
			}
	}

	void Submit(Task task) {
			tasks_.Push(std::move(task));
	}

	void Stop() {
		{
			std::lock_guard<std::mutex> lock(stop_mutex_);
			stop_ = true;
		}
		// Будим всех воркеров, чтобы они завершились
		for (size_t i = 0; i < threads_; ++i) {
			tasks_.Push([]{}); // Пустая задача как сигнал завершения
		}
		for (auto &w : workers_) {
			if (w.joinable()) w.join();
		}
	}

private:
	void WorkerRoutine() {
		while (true) {
			Task task = tasks_.Pop();
			{
				std::lock_guard<std::mutex> lock(stop_mutex_);
				if (stop_) {
					return;
				}
			}
			task();
		}
	}

private:
	size_t threads_;
	std::vector<std::thread> workers_;
	BlockingQueue tasks_;

	bool stop_;
	std::mutex stop_mutex_;
};