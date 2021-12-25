#include <X11/Xlib.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <time.h>
#include <unistd.h>

#define POLL_INTERVAL 50
#define LEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define BLOCK(cmd, interval, signal) {"echo \"$(" cmd ")\"", interval, signal},

typedef struct {
	const char *command;
	const unsigned int interval;
	const unsigned int signal;
} Block;

#include "config.h"

static Display *dpy;
static int screen;
static Window root;

static char outputs[LEN(blocks)][CMDLENGTH + 1 + CLICKABLE_BLOCKS];
static char statusBar[2][LEN(blocks) * ((LEN(outputs[0]) - 1) + (LEN(delimiter) - 1)) + 1];
static struct epoll_event event, events[LEN(blocks) + 2];
static int pipes[LEN(blocks)][2];

static int timerPipe[2];
static int signalFD;
static int epollFD;

static int running = 1;
static int debug = 0;

static int
gcd(int a, int b)
{
	int tmp;
	while (b > 0) {
		tmp = a % b;
		a = b;
		b = tmp;
	}
	return a;
}

void
closePipe(int *pipe)
{
	close(pipe[0]);
	close(pipe[1]);
}

void
execBlock(int i, const char *button)
{
	if (fork() == 0) {
		close(pipes[i][0]);
		dup2(pipes[i][1], STDOUT_FILENO);

		if (button)
			setenv("BLOCK_BUTTON", button, 1);
		execlp(shell, shell, "-c", blocks[i].command, NULL);
		close(pipes[i][1]);
	}
}

void
execBlocks(unsigned long long int time)
{
	int i;
	for (i = 0; i < LEN(blocks); i++)
		if (time == 0 || (blocks[i].interval != 0 && time % blocks[i].interval == 0))
			execBlock(i, NULL);
}

int
getStatus(char *new, char *old)
{
	strcpy(old, new);
	new[0] = '\0';

	for (int i = 0; i < LEN(blocks); i++) {
#ifdef LEADING_DELIMITER
		if (strlen(outputs[i]))
#else
		if (strlen(new) && strlen(outputs[i]))
#endif
			strcat(new, delimiter);
		strcat(new, outputs[i]);
	}
	return strcmp(new, old);
}

void
updateBlock(int i)
{
	char* output = outputs[i];
	char buffer[LEN(outputs[0])];
	int bytesRead = read(pipes[i][0], buffer, LEN(buffer));
	buffer[bytesRead - 1] = '\0';

	if (bytesRead == LEN(buffer)) {
		// Clear the pipe
		char ch;
		while (read(pipes[i][0], &ch, 1) == 1 && ch != '\n')
			;
	} else if (bytesRead == 1) {
		output[0] = '\0';
		return;
	}

#if CLICKABLE_BLOCKS
	if (blocks[i].signal > 0) {
		output[0] = blocks[i].signal;
		output++;
	}
#endif

	strcpy(output, buffer);
}

static void
setroot(void)
{
	// Only set root if text has changed
	if (!getStatus(statusBar[0], statusBar[1]))
		return;

	if (debug) {
		printf("%s\n", statusBar[0]);
		return;
	}

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmblocks: cannot open display");
		exit(1);
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	XStoreName(dpy, root, statusBar[0]);
	XCloseDisplay(dpy);
}

void
signalHandler()
{
	struct signalfd_siginfo info;
	read(signalFD, &info, sizeof(info));

	for (int j = 0; j < LEN(blocks); j++) {
		if (blocks[j].signal == info.ssi_signo - SIGRTMIN) {
			char button[] = {'0' + info.ssi_int & 0xff, 0};
			execBlock(j, button);
			break;
		}
	}

	// Clear the pipe after each poll to limit number of signals handled
	while (read(signalFD, &info, sizeof(info)) != -1);
}

static void
quit(int unused)
{
	running = 0;
	exit(0);
}

static void
setup()
{
	int i;

	epollFD = epoll_create(LEN(blocks) + 1);
	event.events = EPOLLIN;

	for (i = 0; i < LEN(blocks); i++) {
		pipe(pipes[i]);
		event.data.u32 = i;
		epoll_ctl(epollFD, EPOLL_CTL_ADD, pipes[i][0], &event);
	}

	pipe(timerPipe);
	event.data.u32 = LEN(blocks);
	epoll_ctl(epollFD, EPOLL_CTL_ADD, timerPipe[0], &event);

	signal(SIGTERM, quit);
	signal(SIGINT, quit);

	// Avoid zombie subprocesses
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa, 0);

	// Handle block update signals
	sigset_t sigset;
	sigemptyset(&sigset);
	for (int i = 0; i < LEN(blocks); i++)
		if (blocks[i].signal > 0)
			sigaddset(&sigset, SIGRTMIN + blocks[i].signal);

	signalFD = signalfd(-1, &sigset, 0);
	fcntl(signalFD, F_SETFL, O_NONBLOCK);
	sigprocmask(SIG_BLOCK, &sigset, NULL);
	event.data.u32 = LEN(blocks) + 1;
	epoll_ctl(epollFD, EPOLL_CTL_ADD, signalFD, &event);
}

void
run(void)
{
	execBlocks(0);

	while (running) {
		int eventCount = epoll_wait(epollFD, events, LEN(events), POLL_INTERVAL / 10);

		for (int i = 0; i < eventCount; i++) {
			unsigned int id = events[i].data.u32;

			if (id == LEN(blocks)) {
				unsigned long long int j = 0;
				read(timerPipe[0], &j, sizeof(j));
				execBlocks(j);
			} else if (id < LEN(blocks)) {
				updateBlock(events[i].data.u32);
			} else {
				signalHandler();
			}
		}
		if (eventCount)
			setroot();

		// Poll every `POLL_INTERVAL` milliseconds
		struct timespec toSleep = {.tv_nsec = POLL_INTERVAL * 1000000UL};
		nanosleep(&toSleep, &toSleep);
	}
}

void
timerLoop()
{
	close(timerPipe[0]);

	unsigned int sleepInterval = -1;
	for (int i = 0; i < LEN(blocks); i++)
		if (blocks[i].interval)
			sleepInterval = gcd(blocks[i].interval, sleepInterval);

	unsigned long long int i = 0;
	struct timespec sleepTime = {sleepInterval, 0};
	struct timespec toSleep = sleepTime;

	while (1) {
		// Sleep for `sleepTime` even on being interrupted
		if (nanosleep(&toSleep, &toSleep) == -1)
			continue;

		// Notify parent to update blocks
		write(timerPipe[1], &i, sizeof(i));

		// After sleep, reset timer and update counter
		toSleep = sleepTime;
		i += sleepInterval;
	}

	close(timerPipe[1]);
}

void
cleanup(void)
{
	int i;

	close(epollFD);
	close(signalFD);
	close(timerPipe[0]);
	close(timerPipe[1]);
	for (i = 0; i < LEN(pipes); i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
	}
}

int
main(int argc, char *argv[])
{
	int i;

	for (i = 0; i < argc; i++)
		if (!strcmp("-d", argv[i]))
			debug = 1;

	setup();

	if (fork() == 0)
		timerLoop();
	else
		run();

	cleanup();

	return 0;
}
