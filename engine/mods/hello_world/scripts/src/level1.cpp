#include <api.h>
#include "events.hpp"
using namespace api;
extern void do_threads_stuff();
extern void do_remote_stuff();

/* This is the function that gets called at the start.
   See: engine/src/main.cpp */
PUBLIC(void start())
{
    /* 'print' is implemented in api_impl.h, and it makes a system
       call into the engine, which then writes to the terminal. */
    print("Hello world!\n\n");

    print("** Remote **\n");

    /* Do some remote function calls. */
    do_remote_stuff();

    print("** Threads **\n");

    /* Test-run some micro-threads. */
    do_threads_stuff();
}

int main()
{
    print("Hello from Level 1 main()\n");
    return 0;
}
