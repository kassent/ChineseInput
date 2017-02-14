#pragma once
#include "skse\GameAPI.h"

template<typename T, UInt32 N>
class RingBuffer
{
public:
	RingBuffer() : m_head(0)
	{
		ASSERT(N > 0);
		m_pool = FormHeap_Allocate(sizeof(T) * N);
		ASSERT(m_pool != nullptr);
	}

	~RingBuffer()
	{
		FormHeap_Free(m_pool);
	}

	template <typename ... Args>
	T* emplace_back(Args && ... args)
	{
		T* result = nullptr;
		result = new(static_cast<T*>(m_pool) + m_head) T(std::forward <Args>(args)... );
		m_head += 1;
		if (m_head >= N)
			m_head = 0;
		return result;
	}
private:
	void*				m_pool;
	UInt32				m_head;
};