#pragma once
#include <functional>

namespace uloop {

	namespace task {

		size_t size();
		void add(std::function<int()> task, int delay = 0);
	}

	bool is_running();

	void run();
	void exit(int timeout = 50); // small delay to ensure that on-going task has time to finish..
}
