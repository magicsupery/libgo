/************************************************
 * libgo sample1
*************************************************/
#include "coroutine.h"
#include <stdio.h>

void foo()
{
    printf("function pointer\n");
}

int main()
{
    int i;
    go foo;
	co_sched.Start(2);
}

