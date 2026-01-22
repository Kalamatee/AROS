#include <exec/types.h>
#include "defs.h"

#include <proto/kernel.h>

void trapBusError(void)
{
	
	
	
	
}

BOOL initTrapHandlers()
{
	APTR KernelBase = OpenResource("kernel.resource");
	APTR trapHandler = KrnAddExceptionHandler(2, trapBusError, NULL, NULL);
	return (trapHandler != NULL);
}
