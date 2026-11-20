#pragma once

#include "CoreTypes.h"
#include "imgui/imgui.h"

static std::string run_cmd(const std::string& cmd)
{
	char   psBuffer[1024];
	FILE* pPipe;
	std::cout << cmd << std::endl;
	if ((pPipe = _popen(cmd.c_str(), "rb")) == NULL)
		return {};

	std::string content;
	size_t count;
	while (count = fread(psBuffer, 1, sizeof(psBuffer), pPipe)) {
		content += std::string(psBuffer, count);
	}


	if (feof(pPipe))
		_pclose(pPipe);

	return content;

}

extern std::string get_or_create_default_file(int id, const char* path);
extern bool Spinner(const char* label, float radius, int thickness, const ImU32& color);

extern std::string GBKToUTF8(const std::string& gbk_str);
extern std::string UTF8ToGBK(const std::string& gbk_str);
