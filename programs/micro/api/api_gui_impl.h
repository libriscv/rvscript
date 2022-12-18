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

inline GUI::Widget GUI::widget(GUI::Widget& parent)
{
	return {sys_gui_widget(parent.idx)};
}

inline GUI::Button GUI::button(GUI::Widget& parent, const std::string& title)
{
	return {sys_gui_button(parent.idx, title.c_str())};
}

inline GUI::Label GUI::label(GUI::Widget& parent, const std::string& text)
{
	return {sys_gui_label(parent.idx, text.c_str())};
}

inline void GUI::Widget::set_position(int x, int y)
{
	sys_gui_widget_set_pos(this->idx, x, y);
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
