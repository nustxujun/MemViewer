#include "Utils.h"
#include <iostream>
#include <mutex>

static void default_print_log(const std::string& content) {
	static std::mutex s_log_mutex;
	std::lock_guard lock(s_log_mutex);
	std::cerr << content << std::endl;
}

PrintLogFunc g_print_log_func = default_print_log;
