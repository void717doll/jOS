#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "../st.h"
#include "../win.h"

int
main(void)
{
	int mode = 0;

	assert(setvisible(&mode, 1) && (mode & MODE_VISIBLE));
	assert(!setvisible(&mode, 1) && (mode & MODE_VISIBLE));
	assert(!setvisible(&mode, 0) && !(mode & MODE_VISIBLE));
	assert(setvisible(&mode, 1) && (mode & MODE_VISIBLE));
	puts("visibility transitions request a redraw only when shown: passed");
	return 0;
}
