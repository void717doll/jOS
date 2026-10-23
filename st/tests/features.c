#define main resize_checks_main
#include "resize.c"
#undef main

char *urlhandler = "xdg-open";

static void
expect_input(int fd, const char *expected)
{
	char buffer[1024];
	ssize_t size = read(fd, buffer, sizeof buffer - 1);
	assert(size >= 0);
	buffer[size] = '\0';
	assert(!strcmp(buffer, expected));
}

int
main(void)
{
	int pipes[2];
	char *url;
	setlocale(LC_CTYPE, "C.UTF-8");
	tnew(80, 10);
	write_text("猫 http://one.test https://two.test/path\r\n");
	url = urlat(5, 0);
	assert(url && !strcmp(url, "http://one.test"));
	free(url);
	url = urlat(24, 0);
	assert(url && !strcmp(url, "https://two.test/path"));
	free(url);
	assert(!urlat(0, 0) && !urlat(-1, 0) && !urlat(0, term.row));
	highlighturlsline(0);
	assert(TLINE(0)[5].mode & ATTR_URL);
	assert(!(TLINE(0)[0].mode & ATTR_URL));
	write_text("\033[1;1Hplain text\033[K");
	highlighturlsline(0);
	assert(!(TLINE(0)[5].mode & ATTR_URL));

	assert(pipe(pipes) == 0);
	cmdfd = pipes[1];
	assert(fcntl(pipes[0], F_SETFL, O_NONBLOCK) == 0);
	treset();
	write_text("alphabet alphanumeric\r\nal");
	autocomplete(&(Arg){.i = ACMPL_WORD});
	expect_input(pipes[0], "\177\177alphanumeric");
	autocomplete(&(Arg){.i = ACMPL_WORD});
	expect_input(pipes[0], "\177\177\177\177\177\177\177\177\177\177\177\177alphabet");
	autocomplete(&(Arg){.i = ACMPL_UNDO});
	expect_input(pipes[0], "\177\177\177\177\177\177\177\177al");
	assert(completioncount == 0);
	treset();
	write_text("caféine\r\ncaf");
	autocomplete(&(Arg){.i = ACMPL_WORD});
	expect_input(pipes[0], "\177\177\177caféine");
	autocomplete(&(Arg){.i = ACMPL_UNDO});
	expect_input(pipes[0], "\177\177\177\177\177\177\177caf");
	autocomplete(&(Arg){.i = ACMPL_WORD});
	expect_input(pipes[0], "\177\177\177caféine");
	tresize(40, 10);
	assert(completioncount == 0);
	treset();
	tresize(4, 10);
	write_text("你好世界\r\n你好");
	autocomplete(&(Arg){.i = ACMPL_WORD});
	expect_input(pipes[0], "\177\177你好世界");
	autocomplete(&(Arg){.i = ACMPL_UNDO});
	expect_input(pipes[0], "\177\177\177\177你好");
	close(pipes[0]);
	close(pipes[1]);
	puts("url detection, unicode completion, cycling, undo and resize reset: passed");
	return 0;
}
