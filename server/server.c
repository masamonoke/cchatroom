#include "networking/server.h"
#include "clog.h"
#include "shared.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile bool not_interrupted = true;

static void int_handler(int dummy) {
	(void)dummy;
	not_interrupted = false;
}

enum constants { MAX_CLIENTS = 100 };

struct user {
		int         fd;
		const char* username;
		const char* msg[10];
};

struct server_data {
		struct user clients[MAX_CLIENTS];
		uint8_t     capacity;
		uint8_t     size;
};

static void init_user(struct user* usr) {
	usr->fd       = -1;
	usr->username = NULL;
}

static void init_server_data(struct server_data* dat) {
	uint8_t i;

	dat->capacity = MAX_CLIENTS;
	dat->size     = 0;
	for (i = 0; i < dat->capacity; i++) {
		init_user(&dat->clients[i]);
	}
}

static bool is_in(struct server_data* dat, int client_fd) {
	uint8_t i;

	for (i = 0; i < dat->capacity; i++) {
		if (dat->clients[i].fd == client_fd) {
			return true;
		}
	}

	return false;
}

static void insert_client(struct server_data* dat, int client_fd) {
	log_debug("added client");
	dat->clients[dat->size++].fd = client_fd;
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

static bool is_full(struct server_data* dat) {
	return dat->size == dat->capacity;
}

static bool replace_disconnected(struct server_data* dat, int client_fd) {
	uint8_t i;

	for (i = 0; i < dat->size; i++) {
		if (is_disconnected(dat->clients[i].fd)) {
			dat->clients[i].fd = client_fd;
			return true;
		}
	}

	return false;
}

static bool insert(struct server_data* dat, int client_fd) {
	if (!is_in(dat, client_fd)) {
		if (!is_full(dat)) {
			insert_client(dat, client_fd);
		} else {
			if (!replace_disconnected(dat, client_fd)) {
				// server if full
				return false;
			}
		}
	}

	return true;
}

static void prepend_string(const char* prefix, char* dst) {
	size_t len = strlen(prefix);
	memmove(dst + len, dst, strlen(dst) + 1);
	memcpy(dst, prefix, len);
}

// Returning false means disconnection
static bool callback(int client_fd, void* data) {
	struct server_data* dat;
	char                buf[100];
	ssize_t             recevied;
	struct pollfd       pfd = { .fd = client_fd, .events = POLLIN };
	int                 poll_ret;

	// rename to server
	dat = (struct server_data*)data;
	insert(dat, client_fd);

	while (not_interrupted) {
		poll_ret = poll(&pfd, 1, 5000);
		if (poll_ret < 0) {
			return false;
		}

		if (pfd.revents & POLLIN) {
			// check if this client is in server lists
			// if not then add it to
			recevied = recv(client_fd, buf, sizeof(buf), 0);
			if (recevied < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					continue;
				}
				// TODO: log some additional data for client
				log_error("client disconnected");
				return false;
			}
			if (recevied > 0) {
				// before sending append client username like client_username: buf
				buf[recevied] = 0;
				prepend_string("client: ", buf);
				for (uint8_t i = 0; i < dat->size; i++) {
					if (dat->clients[i].fd != client_fd) {
						send(dat->clients[i].fd, buf, strlen(buf) + 1, 0);
					}
				}
			}
		}
	}

	return false;
}

static void close_connections(struct server_data* dat) {
	uint8_t i;

	for (i = 0; i < dat->size; i++) {
		shutdown(dat->clients[i].fd, SHUT_RDWR);
		close(dat->clients[i].fd);
	}
}

int main(void) {
	struct server*     server;
	struct server_data dat;

	if (!networking_server_new(&server, DEFAULT_PORT, callback)) {
		log_error("failed to create server");
		return -1;
	}

	init_server_data(&dat);

	signal(SIGINT, int_handler);
	while (not_interrupted) {
		networking_server_update(server, &dat);
	}

	log_warn("Waiting threads to complete their work");
	networking_server_del(server);

	log_info("Gracefully shutting down");
	close_connections(&dat);

	return 0;
}
