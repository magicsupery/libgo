#include "../../libgo/coroutine.h"


void nano()
{
    DebugPrint(co::dbg_task, "nano function.");
}

void bar()
{
	go nano;
    DebugPrint(co::dbg_task, "bar function.");
	go nano;
}

void foo()
{
	int i = 0;
	while(i < 1)
	{
		go bar;
		i++;
	}
}

int main()
{
	go foo;
	co_sched.Start(2);
}
