#include <setjmp.h>

#include "inferno.h"

jmp_buf LeaveEvents;
int Quitting = 0;
int Screen_mode = -1;
int descent_critical_error = 0;
unsigned int descent_critical_deverror = 0;
unsigned int descent_critical_errcode = 0;

int standard_handler(struct d_event *event)
{
	event = event;
	return 0;
}