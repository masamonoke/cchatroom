#include "packet.h"
#include "control.h"

#include "networking/io.h"
#include "clog.h"

#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

void send_packet(const uint8_t* packet, uint8_t size, int fd) {
	send(fd, &size, sizeof(size), 0);
	if (!networking_io_writen(fd, packet, (size_t)size)) {
		log_error("Failed to write %d bytes", size);
	}
}

enum packet_status recv_packet(uint8_t* packet, uint8_t* size, int fd) {
	ssize_t rv = recv(fd, size, sizeof(*size), 0);
	if (rv < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return PACKET_TRY_AGAIN;
		}
		// TODO: log some additional data for client
		log_error("Failed to read packet size");
		return PACKET_CLIENT_DISCONNECT;
	}

	if (*size == 0) {
		return PACKET_CLIENT_DISCONNECT;
	}

	if (*size > MAX_PACKET_SIZE) {
		if (networking_io_readn(fd, packet, MAX_PACKET_SIZE) != NETWORKING_IO_READ_ALL) {
			log_error("Failed to read packet's %d bytes", *size);
		}
		return PACKET_SIZE_LIMIT_EXCEED;
	}

	if (networking_io_readn(fd, packet, *size) != NETWORKING_IO_READ_ALL) {
		log_error("Failed to read packet's %d bytes", *size);
		return PACKET_CLIENT_DISCONNECT;
	}

	return PACKET_OK;
}

__attribute__((nonnull(1, 2)))
static void append_data(uint8_t** dest, const void* src, uint8_t size) {
	memcpy(*dest, src, size);
	*dest += size;
}

static void build_packet(uint8_t* packet, uint8_t id, uint8_t cmd, uint8_t payload_size,
                         const uint8_t* payload) {
	uint8_t* p = packet;

	append_data(&p, &id, sizeof(id));
	append_data(&p, &cmd, sizeof(cmd));
	append_data(&p, &payload_size, sizeof(payload_size));
	append_data(&p, payload, payload_size);
}

uint8_t* make_cmd_packet(cchatroom_cmd_t cmd, const uint8_t* payload, uint8_t payload_size, uint8_t* packet_size_out) {
	static uint8_t packet[MAX_PACKET_SIZE];
	static uint8_t  packet_size = 0;

	switch (cmd) {
		case COMMAND_BROADCAST:
		{
			// TODO: probably this block is general for all commands
			// TODO: where to get it?
			uint8_t id = 1;
			uint8_t msg_size = (uint8_t)(sizeof(id) + sizeof(cmd) + sizeof(payload_size)) + payload_size;

			packet_size = msg_size;

			build_packet(packet, id, cmd, payload_size, payload);
		}
		break;
		default:
			not_implemented();
			break;
	};

	*packet_size_out = packet_size;
	return packet;
}

static void pop_data(uint8_t* dest, uint8_t** src, size_t size) {
	memcpy(dest, *src, size);
	*src += size;
}

bool parse_packet(uint8_t* packet, uint8_t size, struct packet* out_packet) {
	uint8_t* p = packet;
	uint8_t real_size = 0;

	pop_data(&out_packet->id, &p, sizeof(out_packet->id));
	pop_data(&out_packet->cmd, &p, sizeof(out_packet->cmd));
	pop_data(&out_packet->payload_size, &p, sizeof(out_packet->payload_size));
	pop_data(out_packet->payload, &p, out_packet->payload_size);

	real_size += sizeof(out_packet->id);
	real_size += sizeof(out_packet->cmd);
	real_size += sizeof(out_packet->payload_size);
	real_size += out_packet->payload_size;

	if (real_size != size) {
		log_error("Actual packet size %d, expected %d", real_size, size);
		return false;
	}

	return true;
}
