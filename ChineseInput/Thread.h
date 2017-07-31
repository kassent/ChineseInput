#pragma once

#include <future>

template <typename F, typename... Args>
auto really_async(F&& f, Args&&... args)-> std::future<typename std::result_of<F(Args...)>::type>
{
	using _Ret = typename std::result_of<F(Args...)>::type;
	auto _func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
	std::packaged_task<_Ret()> task(std::move(_func));
	auto _fut = task.get_future();
	std::thread thread(std::move(task));
	thread.detach();
	return _fut;
}