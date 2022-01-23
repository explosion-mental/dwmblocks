#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <X11/Xlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LEN(X)			(sizeof(X) / sizeof(X[0]))
#define MAX(A, B)               ((A) > (B) ? (A) : (B))

typedef struct {
	const char *command;
	const unsigned int interval;
	const unsigned int signal;
} Block;

#include "config.h"

/* variables */
static Display *dpy;
static int screen;
static Window root;

static char outputs[LEN(blocks)][CMDLENGTH + 2];
static char status[2][LEN(blocks) * ((LEN(outputs[0]) - 1) + (LEN(delimiter) - 1)) + 1];
static struct epoll_event event, events[LEN(blocks) + 2];
static int signalFD, epollFD;			/* file descriptors */
static int pipes[LEN(blocks)][2], timer[2];	/* pipes */

static int running = 1;
static int printstdout = 0;
static unsigned int lock = 0;

static void
cleanup(void)
{
	unsigned int i;

	/* close pipes */
	for (i = 0; i < LEN(pipes); i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
	}

	/* close file descriptors */
	close(epollFD);
	close(signalFD);
	/* close timers */
	close(timer[0]);
	close(timer[1]);
}
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

static void
execBlock(int i, const char *button)
{
	// Ensure only one child process exists per block at an instance
	if (lock & 1 << i)
		return;
	// Lock execution of block until current instance finishes execution
	lock |= 1 << i;

	if (fork() == 0) {
		close(pipes[i][0]);
		dup2(pipes[i][1], STDOUT_FILENO);
		close(pipes[i][1]);
		if (button)
			setenv("BLOCK_BUTTON", button, 1);
		execlp(shell, shell, "-c", blocks[i].command, NULL);
		exit(EXIT_SUCCESS);
	}
}

static void
execBlocks(unsigned int time)
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

static int
getstatus(char *new, char *old)
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

static void
updateBlock(int i)
{
	char buffer[LEN(outputs[0])];
	char *output = outputs[i];
	int bytesread = read(pipes[i][0], buffer, LEN(buffer));

	//buffer[bytesread - 1] = '\0';
	// Trim UTF-8 characters properly
	int j = bytesread - 1;
	while ((buffer[j] & 0b11000000) == 0x80)
		j--;
	//buffer[j] = '\0';
	buffer[j] = ' ';
	// Trim trailing spaces
	while (buffer[j] == ' ')
		j--;
	buffer[j + 1] = '\0';



	if (bytesread == LEN(buffer)) { /* clear the pipe */
		char ch;
		while (read(pipes[i][0], &ch, 1) == 1 && ch != '\n');
	}

	if (clickable && (bytesread > 1 && blocks[i].signal > 0)) {
		output[0] = blocks[i].signal;
		output++;
	}

	strcpy(output, buffer);

	// Remove execution lock for the current block
	lock &= ~(1 << i);
}

static void
setroot(void)
{
	/* Only set root if text has changed */
	if (!getstatus(status[0], status[1]))
		return;

	if (printstdout) {
		printf("%s\n", status[0]);
		return;
	}

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmblocks: cannot open display");
		exit(1);
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	XStoreName(dpy, root, status[0]);
	XCloseDisplay(dpy);
}

static void
signalHandler(void)
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
			char button[] = {'0' + info.ssi_int & 0xff, '\0'};
			execBlock(i, button);
			break;
		}
	}
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
	sigaction(SIGCHLD, &sa, NULL);
	//if (signal(SIGCHLD, sigchld) == SIG_ERR) {
	//	fprintf(stderr, "can't install SIGCHLD handler:");
	//	exit(1);
	//}
	//while (0 < waitpid(-1, NULL, WNOHANG));
}

static void
setup(void)
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

	// Ignore all realtime signals
	sigset_t ignoredSignals;
	sigemptyset(&ignoredSignals);
	for (i = SIGRTMIN; i <= SIGRTMAX; i++)
		sigaddset(&ignoredSignals, i);
	sigprocmask(SIG_BLOCK, &ignoredSignals, NULL);

	signal(SIGTERM, quit);
	signal(SIGINT, quit);

	/* Handle block update signals */
	sigset_t handledSignals;
	sigemptyset(&handledSignals);
#ifdef INVERSED
	for (i = LEN(blocks) - 1; i >= 0; i--)
#else
	for (i = 0; i < LEN(blocks); i++)
#endif /* INVERSED */
		if (blocks[i].signal > 0) {
		sigaddset(&handledSignals, SIGRTMIN + blocks[i].signal);
		}
	signalFD = signalfd(-1, &handledSignals, 0);

	event.data.u32 = LEN(blocks) + 1;
	epoll_ctl(epollFD, EPOLL_CTL_ADD, signalFD, &event);
}

static void
statusloop(void)
{
	unsigned int j;
	//struct timespec tosleep = {.tv_nsec = POLL_INTERVAL * 1000000UL};
	int eventCount;
	int i;

	while (running) {
 		eventCount = epoll_wait(epollFD, events, LEN(events), -1);

		for (i = 0; i < eventCount; i++) {
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

		if (eventCount != -1)
			setroot();
	}
}

static void
timerloop()
{
	int i;
	unsigned int interval = 0, maxinterval = 0;

	close(timer[0]);
#ifdef INVERSED
	for (i = LEN(blocks) - 1; i >= 0; i--)
#else
	for (i = 0; i < LEN(blocks); i++)
#endif /* INVERSED */
		if (blocks[i].interval) {
			maxinterval = MAX(blocks[i].interval, maxinterval);
			interval = gcd(blocks[i].interval, interval);
		}

	unsigned int count = 0;
	struct timespec sleeptime = {interval, 0};
	struct timespec tosleep = sleeptime;

	while (running) {
		/* notify parent (statusloop) to update blocks */
		write(timer[1], &count, sizeof(count));

 		/* update counter */
		// Wrap `i` to the interval [1, maxInterval]
		count = (count + interval - 1) % maxinterval + 1;

		// Sleep for `sleepTime` even on being interrupted
		while (nanosleep(&tosleep, &tosleep) == -1);
		tosleep = sleeptime;
	}

	close(timer[1]);
	exit(EXIT_SUCCESS);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-p", argv[1]))
		printstdout = 1;
	else if (argc != 1) {
		puts("usage: dwmblocks [-p]");
		exit(1);
	}

	setup();

	if (fork() == 0) /* child */
		timerloop();
	else /* parent */
		statusloop();

	cleanup();

	return 0;
}
