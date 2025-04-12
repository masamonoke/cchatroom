#pragma once

#include "command.h"

#include <stdbool.h>
#include <stdint.h>

enum packet_constans { MAX_PACKET_SIZE = 252, MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - 3 };

enum packet_status {
	PACKET_TRY_AGAIN,
	PACKET_OK,
	PACKET_CLIENT_DISCONNECT,
	PACKET_SIZE_LIMIT_EXCEED,
};

struct packet {
	uint8_t         id;
	cchatroom_cmd_t cmd;
	uint8_t         payload[MAX_PAYLOAD_SIZE];
	uint8_t         payload_size;
};

__attribute__((nonnull(1)))
void send_packet(const uint8_t* packet, uint8_t size, int fd);

__attribute__((nonnull(1, 2)))
enum packet_status recv_packet(uint8_t* packet, uint8_t* size, int fd);

// TODO: make packets only once and then reuse them only substituing data or realloc if needed
// Mallocs memory
// TODO: does this attr work?
__attribute__((malloc, nonnull(2, 4)))
uint8_t* make_cmd_packet(cchatroom_cmd_t cmd, const uint8_t* payload,
        				 uint8_t payload_size, uint8_t* packet_size_out);

__attribute__((nonnull(1, 3)))
bool parse_packet(uint8_t* packet, uint8_t size, struct packet* out_packet);
