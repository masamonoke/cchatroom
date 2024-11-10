#pragma once

// TODO: check if it is packed
typedef enum __attribute__((packed)) {
	COMMAND_AUTHORIZE,
	COMMAND_BROADCAST
} cchatroom_cmd_t;
