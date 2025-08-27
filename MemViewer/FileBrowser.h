#pragma once

#include <vector>
#include <string>
#include <functional>
#include <memory>

class FileBrowserView
{
public :
	const std::string& getPath();

	void ShowFileBrowser(std::function<void(std::string, int)>&& callback);

	FileBrowserView();
	bool isLoading(){return *is_running;}
private:
	std::vector<std::string> file_list;
	int selected_file = -1;
	std::shared_ptr<bool> is_running ;
};