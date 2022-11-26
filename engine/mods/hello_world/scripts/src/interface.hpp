#pragma once

struct Gameplay {
    bool action = false;

    void set_action(bool a);
    bool get_action();

    void do_nothing();
};
extern Gameplay gameplay_state;
