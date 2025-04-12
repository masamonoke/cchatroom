#pragma once

typedef enum __attribute__((packed)) {
	COMMAND_HAND_SHAKE,
	COMMAND_AUTHORIZE,
	COMMAND_BROADCAST
} cchatroom_cmd_t;
