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
		{"GUI::find",
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
		{"GUI::window",
		 [&screen](Script& script)
		 {
			 const auto [title] = script.machine().sysargs<std::string>();

			 auto* wnd = new Window(&screen, title);
			 wnd->set_layout(new GroupLayout());
			 auto idx = screen.managew(wnd);

			 script.machine().set_result(idx);
		 }},
		{"GUI::button",
		 [&screen](Script& script)
		 {
			 const auto [widx, text]
				 = script.machine().sysargs<size_t, std::string>();

			 auto* parent = screen.getw(widx);

			 auto* button = new Button(parent, text);
			 auto idx	  = screen.managew(button);

			 script.machine().set_result(idx);
		 }},
		{"GUI::label",
		 [&screen](Script& script)
		 {
			 const auto [widx, text]
				 = script.machine().sysargs<size_t, std::string>();

			 auto* parent = screen.getw(widx);

			 auto* label = new Label(parent, text);
			 auto idx	 = screen.managew(label);

			 script.machine().set_result(idx);
		 }},
		{"GUI::widget_set_pos",
		 [&screen](Script& script)
		 {
			 const auto [widx, x, y]
				 = script.machine().sysargs<size_t, int, int>();

			 nanogui::Widget* widget = screen.getw(widx);
			 widget->set_position({x, y});
		 }},
		{"GUI::widget_callback",
		 [&screen](Script& script)
		 {
			 auto& machine = script.machine();
			 const auto [widx, callback, data, size]
				 = machine.sysargs<unsigned, gaddr_t, gaddr_t, gaddr_t>();

			 auto capture = CaptureStorage::get(machine, data, size);
			 auto func = [callback = callback, widx = widx, capture, &script]
			 {
				 gaddr_t dst = script.guest_alloc(capture.size());
				 script.machine().copy_to_guest(
					 dst, capture.data(), capture.size());
				 script.call(callback, widx, dst);
				 script.guest_free(dst);
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
