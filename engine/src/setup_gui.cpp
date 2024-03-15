#include "main_screen.hpp"
#include <nanogui/nanogui.h>
#include <script/helpers.hpp>
#include <script/script.hpp>
using namespace nanogui;

/** GUI dynamic calls **/
void setup_gui_system(MainScreen& screen)
{
	using gaddr_t = Script::gaddr_t;

	Script::set_dynamic_calls({
		{"GUI::find", "unsigned sys_gui_find   (const char *)",
		 [&screen](Script& script)
		 {
			 const auto [text] = script.machine().sysargs<std::string>();

			 for (auto* widget : screen.children())
			 {
				 if (typeid(*widget) == typeid(nanogui::Window))
				 {
					 auto* window = (Window*)widget;
					 if (window->title() == text)
					 {
						 script.machine().set_result(screen.getw(window));
						 return;
					 }
				 }
			 }

			 script.machine().set_result(-1);
		 }},
		{"GUI::window", "unsigned sys_gui_window (const char *)",
		 [&screen](Script& script)
		 {
			 const auto [title] = script.machine().sysargs<std::string>();

			 auto* wnd = new Window(&screen, title);
			 wnd->set_layout(new GroupLayout());
			 auto idx = screen.managew(wnd);

			 script.machine().set_result(idx);
		 }},
		{"GUI::button", "unsigned sys_gui_button (unsigned, const char *)",
		 [&screen](Script& script)
		 {
			 const auto [widx, text]
				 = script.machine().sysargs<size_t, std::string>();

			 auto* parent = screen.getw(widx);

			 auto* button = new Button(parent, text);
			 auto idx	  = screen.managew(button);

			 script.machine().set_result(idx);
		 }},
		{"GUI::label", "unsigned sys_gui_label (unsigned, const char *)",
		 [&screen](Script& script)
		 {
			 const auto [widx, text]
				 = script.machine().sysargs<size_t, std::string>();

			 auto* parent = screen.getw(widx);

			 auto* label = new Label(parent, text);
			 auto idx	 = screen.managew(label);

			 script.machine().set_result(idx);
		 }},
		{"GUI::widget_set_pos", "void sys_gui_widget_set_pos (unsigned, int, int)",
		 [&screen](Script& script)
		 {
			 const auto [widx, x, y]
				 = script.machine().sysargs<size_t, int, int>();

			 nanogui::Widget* widget = screen.getw(widx);
			 widget->set_position({x, y});
		 }},
		{"GUI::widget_callback", "void sys_gui_widget_callback (unsigned, gui_callback, void *, size_t)",
		 [&screen](Script& script)
		 {
			 auto& machine = script.machine();
			 const auto [widx, callback, data, size]
				 = machine.sysargs<unsigned, gaddr_t, gaddr_t, gaddr_t>();

			 auto capture = CaptureStorage::get(machine, data, size);
			 auto func = [callback = callback, widx = widx, capture, &script]
			 {
				 script.call(callback, widx, capture);
			 };

			 nanogui::Widget& widget = *screen.getw(widx);
			 if (typeid(widget) == typeid(Button))
			 {
				 dynamic_cast<Button&>(widget).set_callback(std::move(func));
			 }
			 else
			 {
				 throw std::runtime_error(
					 "Exception: Unsupported widget type: "
					 + std::string(typeid(widget).name()));
			 }
		 }},
	});
}
