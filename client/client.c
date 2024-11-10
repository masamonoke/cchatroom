#include "control.h"
#include "packet.h"
#include "shared.h"

#include "clog.h"
#include "networking/io.h"
#include "networking/socket.h"

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ERASE_LINE "\033[2K\r"

const int MAX_MSG_SIZE            = MAX_PAYLOAD_SIZE + 1;
const int MAX_BUF_SIZE            = MAX_MSG_SIZE * 5;
const int INPUT_WAIT_TIME_MINUTES = 10;

// NOTE: access this variable only through check_running() and set_running() macros
static volatile int keep_running = 1;

static pthread_mutex_t keep_running_mutex;

#define check_running(cond, action)                                                                                    \
	if (keep_running == (cond)) {                                                                                      \
		pthread_mutex_lock(&keep_running_mutex);                                                                       \
		action                                                                                                         \
		pthread_mutex_unlock(&keep_running_mutex);                                                                     \
	}

static inline void set_running(int val) {
	pthread_mutex_lock(&keep_running_mutex);
	keep_running = (val);
	pthread_mutex_unlock(&keep_running_mutex);
}

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

static void clear_input(void) {
	fprintf(stderr, ERASE_LINE);
}

static void print_prefix(void) {
	fprintf(stderr, "(you): ");
}

static bool get_stdin(char* buf, int size) {
	if (buf == NULL || size == 0) {
		return false;
	}

	fd_set         set;
	struct timeval timeout;

	FD_ZERO(&set);
	FD_SET(STDIN_FILENO, &set);

	int seconds     = INPUT_WAIT_TIME_MINUTES * 60;
	timeout.tv_sec  = seconds;
	timeout.tv_usec = 0;

	while (keep_running) {
		print_prefix();
		int result = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);

		if (result == 0) {
			log_warn("You input nothing within %d seconds. The program is closing", seconds);
			return false;
		}

		if (fgets(buf, size, stdin) != NULL) {
			int len = (int)strlen(buf);

			if (len > MAX_MSG_SIZE) {
				log_warn("You entered %d characters, that is more than limit: %d. Your message won't be sent", len,
				         MAX_MSG_SIZE);
				buf[0] = '\0';
			}

			buf[strlen(buf) - 1] = '\0';

			return true;
		} else {
			return false;
		}
	}

	return false;
}

static void print_response(const char* resp) {
	clear_input();
	printf("%s\n", resp);
	print_prefix();
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
				fprintf(stderr, ERASE_LINE);
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

	char     stdin_buf[MAX_BUF_SIZE];
	uint8_t* packet;
	uint8_t  packet_size;

	signal(SIGINT, int_handler);
	pthread_t poll_thread;
	pthread_create(&poll_thread, NULL, poll_server, &pfd);

	while (keep_running) {
		if (!get_stdin(stdin_buf, sizeof(stdin_buf))) {
			set_running(false);
			break;
		}

		size_t msg_len = strlen(stdin_buf);
		if (msg_len > 0) {
			if (msg_len > MAX_PAYLOAD_SIZE) {
				log_error("Too big message");
			} else {
				packet = make_cmd_packet(COMMAND_BROADCAST, (uint8_t*)stdin_buf, (uint8_t)msg_len + 1, &packet_size);
				send_packet(packet, packet_size, server_fd);
			}
		}
	}

	shutdown(server_fd, SHUT_RDWR);
	close(server_fd);
	log_info("Closed connection");

	pthread_join(poll_thread, NULL);

	log_info("Gracefully shutting down");

	return 0;
}
