#pragma once
#include <iostream>
#include <atomic>
#include <vector>

template<typename T>
class spsc {
	size_t	m_capacity{};
	std::atomic<size_t>	m_head{};
	std::atomic<size_t>	m_tail{};
	std::vector<T>	m_buffer{};
	public:
		spsc(size_t	capacity = 10) : m_capacity{capacity}, m_buffer(capacity) {}
		size_t capacity() const { return m_capacity; }
		bool enqueue(const T& item) {
			size_t	t = m_tail.load(std::memory_order_relaxed);	
			size_t	next = (t + 1) % capacity();
			if (next == m_head.load(std::memory_order_acquire))  return false;
			m_buffer[t] = item;
			m_tail.store(next, std::memory_order_release);
			return true;
		}

		bool dequeue(T& item) {
			size_t	h = m_head.load(std::memory_order_relaxed);
			if (h == m_tail.load(std::memory_order_acquire))
				return false;
			item = std::move(m_buffer[h]);
			m_head.store((h + 1)%capacity(), std::memory_order_acquire);
			return true;
		}
};
