#pragma once

/** GUI **/

inline guidx_t GUI::find(const std::string& title)
{
	return sys_gui_find(title.c_str());
}

inline GUI::Window GUI::window(const std::string& title)
{
	return {sys_gui_window(title.c_str())};
}

inline GUI::Button GUI::button(GUI::Widget& parent, const std::string& title)
{
	return {sys_gui_button(parent.idx, title.c_str())};
}

inline void GUI::Widget::set_callback(Function<void()> callback)
{
	return sys_gui_widget_callback(
		this->idx,
		[](unsigned idx, void* data)
		{
			(void)idx;
			(*(decltype(&callback))data)();
		},
		&callback, sizeof(callback));
}
