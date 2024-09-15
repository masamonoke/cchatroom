#include "control.h"
#include "shared.h"

#include "clog.h"
#include "networking/socket.h"

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// NOTE: access this variable only through check_running() and set_running() macros
static volatile int keep_running = 1;

static pthread_mutex_t keep_running_mutex;

#define check_running(cond, action)                                                                                    \
	if (keep_running == (cond)) {                                                                                      \
		pthread_mutex_lock(&keep_running_mutex);                                                                       \
		action                                                                                                         \
		pthread_mutex_unlock(&keep_running_mutex);                                                                     \
	}

#define set_running(val)                                                                                               \
	pthread_mutex_lock(&keep_running_mutex);                                                                           \
	keep_running = (val);                                                                                              \
	pthread_mutex_unlock(&keep_running_mutex);

static void int_handler(int dummy) {
	(void)dummy;
	keep_running = 0;
}

static bool is_disconnected(int client_fd) {
	char    buf;
	ssize_t ret;

	ret = recv(client_fd, &buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);

	if (ret == 0) {
		return true;
	} else if (ret < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return false;
		} else {
			// some kind of error which might indicate disconnection
			return true;
		}
	}

	return false;
}

static bool get_stdin(char* buf, size_t size) {
	size_t        cnt;
	char          c;
	struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
	int           poll_ret;

	cnt = 0;
	if (buf == NULL || size == 0) {
		return false;
	}

	while (keep_running) {
		poll_ret = poll(&pfd, 1, 5000);
		if (poll_ret < 0) {
			break;
		}

		if (pfd.revents & POLLIN) {
			if (!(read(STDIN_FILENO, &c, 1) == 1 && cnt < size - 1)) {
				break;
			}
			if (c == '\n') {
				buf[cnt] = 0;
				return true;
			}
			buf[cnt++] = c;
		}
	}

	buf[cnt] = 0;
	return false;
}

enum client_cmd { CLIENT_EXIT_CMD, CLIENT_UNKNOWN_CMD_OR_INPUT };

// String must be null terminated
static enum client_cmd parse_input(const char* str) {
	if (0 == strcmp(str, "exit")) {
		return CLIENT_EXIT_CMD;
	}

	return CLIENT_UNKNOWN_CMD_OR_INPUT;
}

static void print_response(const char* resp) {
	printf("%s\n", resp);
}

static void* poll_server(void* arg) {
	struct pollfd* pfd = (struct pollfd*)arg;
	int            poll_ret;
	char           server_resp_buf[100];

	while (true) {
		check_running(false, { break; });
		poll_ret = poll(pfd, 1, 5000);
		if (poll_ret < 0) {
			log_error("Failed to poll server");
			break;
		}

		if (pfd->revents & POLLIN) {
			if (!is_disconnected(pfd->fd)) {
				recv(pfd->fd, &server_resp_buf, sizeof(server_resp_buf), 0);
			} else {
				log_warn("You were disconnected");
				set_running(false);
				break;
			}
			check_running(true, { print_response(server_resp_buf); });
		}
	}

	return NULL;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		die("usage: cchatroom-client address");
	}

	int         server_fd;
	const char* address = argv[1];
	if (!networking_socket_create_client(&server_fd, address, DEFAULT_PORT, SOCK_STREAM)) {
		log_info("Failed to create cchatroom connection on %s:%s", address, DEFAULT_PORT);
		return 1;
	}

	struct pollfd pfd = { .fd = server_fd, .events = POLLIN };

	char stdin_buf[100];

	signal(SIGINT, int_handler);
	pthread_t poll_thread;
	pthread_create(&poll_thread, NULL, poll_server, &pfd);

	while (keep_running) {
		// BUG: when server disconnect client you need to press enter to see disconnected message
		if (!get_stdin(stdin_buf, sizeof(stdin_buf))) {
			set_running(false);
			break;
		}

		if (parse_input(stdin_buf) == CLIENT_EXIT_CMD) {
			set_running(false);
			break;
		}

		if (is_disconnected(server_fd)) {
			log_info("Server has disconnected you");
			set_running(false);
			break;
		}
		if (strlen(stdin_buf) > 0) {
			send(server_fd, stdin_buf, strlen(stdin_buf) + 1, 0);
		}
	}

	shutdown(server_fd, SHUT_RDWR);
	close(server_fd);

	pthread_join(poll_thread, NULL);

	log_info("Gracefully shutting down");

	return 0;
}
