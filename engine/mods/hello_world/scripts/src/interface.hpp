#pragma once

struct Gameplay {
    bool action = false;

    void set_action(bool a);
    bool get_action();
};
extern Gameplay gameplay_state;
