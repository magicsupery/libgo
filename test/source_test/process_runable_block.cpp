#include "../../libgo/coroutine.h"


void block()
{
    DebugPrint(co::dbg_task, "block function.");
	sleep(10);
}


void runable()
{
    co_yield;
}



void done()
{
    DebugPrint(co::dbg_task, "done function.");
}

void foo()
{
	go block;
    go runable;
	go done;
	
}

int main()
{
	go foo;
	co_sched.Start();
}
