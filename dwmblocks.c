#include <X11/Xlib.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <time.h>
#include <wait.h>
#include <unistd.h>

#define LEN(X) (sizeof(X) / sizeof(X[0]))

typedef struct {
	const char *command;
	const unsigned int interval;
	const unsigned int signal;
} Block;

#include "config.h"

static Display *dpy;
static int screen;
static Window root;

static char outputs[LEN(blocks)][CMDLENGTH + 2];
static char statusBar[2][LEN(blocks) * ((LEN(outputs[0]) - 1) + (LEN(delimiter) - 1)) + 1];

static struct epoll_event event, events[LEN(blocks) + 2];
static int signalFD, epollFD;

/* pipes */
static int pipes[LEN(blocks)][2];
static int timer[2];

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
#ifdef INVERSED
	for (i = LEN(blocks) - 1; i >= 0; i--)
#else
	for (i = 0; i < LEN(blocks); i++)
#endif /* INVERSED */
		if (time == 0 || (blocks[i].interval != 0 && time % blocks[i].interval == 0))
			execBlock(i, NULL);
}

int
getStatus(char *new, char *old)
{
	int i;
	strcpy(old, new);
	new[0] = '\0';

#ifdef INVERSED
	for (i = LEN(blocks) - 1; i >= 0; i--) {
#else
	for (i = 0; i < LEN(blocks); i++) {
#endif /* INVERSED */
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
	char buffer[LEN(outputs[0])];
	char *output = outputs[i];
	int bytesread = read(pipes[i][0], buffer, LEN(buffer));
	buffer[bytesread - 1] = '\0';

	if (bytesread == LEN(buffer)) { /* clear the pipe */
		char ch;
		while (read(pipes[i][0], &ch, 1) == 1 && ch != '\n');
	} else if (bytesread == 1) {
		output[0] = '\0';
		return;
	}

	if (clickable && blocks[i].signal > 0) {
		output[0] = blocks[i].signal;
		output++;
	}

	strcpy(output, buffer);
}

static void
setroot(void)
{
	/* Only set root if text has changed */
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
	int i;
	struct signalfd_siginfo info;
	read(signalFD, &info, sizeof(info));

#ifdef INVERSED
	for (i = LEN(blocks) - 1; i >= 0; i--) {
#else
	for (i = 0; i < LEN(blocks); i++) {
#endif /* INVERSED */
		if (blocks[i].signal == info.ssi_signo - SIGRTMIN) {
			char button[] = {'0' + info.ssi_int & 0xff, 0};
			execBlock(i, button);
			break;
		}
	}

	/* clear the pipe after each poll to limit number of signals handled */
	while (read(signalFD, &info, sizeof(info)) != -1);
}

static void
quit(int unused)
{
	running = 0;
	//exit(0);
}

static void
sigchld(int unused)
{
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa, 0);
	//if (signal(SIGCHLD, sigchld) == SIG_ERR) {
	//	fprintf(stderr, "can't install SIGCHLD handler:");
	//	exit(1);
	//}
	//while (0 < waitpid(-1, NULL, WNOHANG));
}

static void
setup()
{
	int i;

	epollFD = epoll_create(LEN(blocks) + 1);
	event.events = EPOLLIN;

	/* init pipes */
#ifdef INVERSED
	for (i = LEN(blocks) - 1; i >= 0; i--) {
#else
	for (i = 0; i < LEN(blocks); i++) {
#endif /* INVERSED */
		pipe(pipes[i]);
		event.data.u32 = i;
		epoll_ctl(epollFD, EPOLL_CTL_ADD, pipes[i][0], &event);
	}

	/* timer pipe */
	pipe(timer);
	event.data.u32 = LEN(blocks);
	epoll_ctl(epollFD, EPOLL_CTL_ADD, timer[0], &event);

	/* clean up any zombies immediately */
	sigchld(0);

	signal(SIGTERM, quit);
	signal(SIGINT, quit);

	// Handle block update signals
	sigset_t sigset;
	sigemptyset(&sigset);
#ifdef INVERSED
	for (i = LEN(blocks) - 1; i >= 0; i--)
#else
	for (i = 0; i < LEN(blocks); i++)
#endif /* INVERSED */
		if (blocks[i].signal > 0) {
			sigaddset(&sigset, SIGRTMIN + blocks[i].signal);
		}

	signalFD = signalfd(-1, &sigset, 0);
	fcntl(signalFD, F_SETFL, O_NONBLOCK);
	sigprocmask(SIG_BLOCK, &sigset, NULL);
	event.data.u32 = LEN(blocks) + 1;
	epoll_ctl(epollFD, EPOLL_CTL_ADD, signalFD, &event);
}

static void
statusloop(void)
{
	unsigned long long int j;
	execBlocks(0);

	while (running) {
		int eventCount = epoll_wait(epollFD, events, LEN(events), POLL_INTERVAL / 10);

		for (int i = 0; i < eventCount; i++) {
			unsigned int id = events[i].data.u32;

			if (id == LEN(blocks)) {
				j = 0;
				read(timer[0], &j, sizeof(j));
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

static void
timerloop()
{
	int j;
	close(timer[0]);

	unsigned int sleepInterval = -1;
#ifdef INVERSED
	for (j = LEN(blocks) - 1; j >= 0; j--)
#else
	for (j = 0; j < LEN(blocks); j++)
#endif /* INVERSED */
		if (blocks[j].interval)
			sleepInterval = gcd(blocks[j].interval, sleepInterval);

	unsigned long long int i = 0;
	struct timespec sleepTime = {sleepInterval, 0};
	struct timespec toSleep = sleepTime;

	while (1) {
		// Sleep for `sleepTime` even on being interrupted
		if (nanosleep(&toSleep, &toSleep) == -1)
			continue;

		// Notify parent to update blocks
		write(timer[1], &i, sizeof(i));

		// After sleep, reset timer and update counter
		toSleep = sleepTime;
		i += sleepInterval;
	}

	close(timer[1]);
}

static void
cleanup(void)
{
	int i;

	for (i = 0; i < LEN(pipes); i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
	}

	close(epollFD);
	close(signalFD);
	close(timer[0]);
	close(timer[1]);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-d", argv[1]))
		debug = 1;
	else if (argc != 1) {
		puts("usage: dwmblocks [-d]");
		exit(1);
	}

	setup();

	if (fork() == 0)
		timerloop();
	else
		statusloop();

	cleanup();

	return 0;
}
