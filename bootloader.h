/*
 * bootloader.h
 *
 *  Created on: Nov 13, 2023
 *      Author: tyuga
 */

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

#include <stdint.h>
#include <stdbool.h>
#include "bsp_bootloader.h"

#define MAGIC_NUMBER 0x29BAF059

#define BOOTLOADER_SOF                 0xAA
#define BOOTLOADER_EOF                 0xBB
#define BOOTLOADER_ACK                 0x00
#define BOOTLOADER_NACK                0x01

#define BOOTLOADER_DATA_OVERHEAD_SIZE  9
#define BOOTLOADER_DATA_MAX_SIZE       1024
#define BOOTLOADER_PACKET_MAX_SIZE     (BOOTLOADER_DATA_OVERHEAD_SIZE + BOOTLOADER_DATA_MAX_SIZE)

/**
 * @brief Exception code
 * 
 */
typedef enum {
    BOOTLOADER_OK = 0,
    BOOTLOADER_ERROR = 0xFF,
} bootloader_exception_t;

typedef enum {
    BOOTLOADER_STATE_IDLE,
    BOOTLOADER_STATE_START,
    BOOTLOADER_STATE_HEADER,
    BOOTLOADER_STATE_DATA,
    BOOTLOADER_STATE_END,
} bootloader_state_t;

typedef enum {
    BOOTLOADER_PACKET_TYPE_CMD,
    BOOTLOADER_PACKET_TYPE_DATA,
    BOOTLOADER_PACKET_TYPE_HEADER,
    BOOTLOADER_PACKET_TYPE_RESPONSE,
} bootloader_packet_type_t;

typedef enum {
    BOOTLOADER_CMD_START,
    BOOTLOADER_CMD_END,
    BOOTLOADER_CMD_ABORT,
} bootloader_command_t;

/*
 * Slot table
 */
typedef struct {
    uint8_t  valid;
    uint8_t  active;
    uint32_t size;
    uint32_t crc;
}__attribute__((packed)) slot_t;

/*
 * General configuration
 */
typedef struct {
    uint32_t reboot;
	uint32_t magicNumber;
    slot_t slot[NUM_OF_SLOTS];
}__attribute__((packed)) slot_cfg_t;

/*
 * OTA meta info
 */
typedef struct {
	uint32_t package_size;
	uint32_t package_crc;
	uint32_t reserved1;
	uint32_t reserved2;
}__attribute__((packed)) meta_info;

/*
 * OTA Command format
 * ________________________________________
 * |     | Packet |     |     |     |     |
 * | SOF | Type   | Len | CMD | CRC | EOF |
 * |_____|________|_____|_____|_____|_____|
 *   1B      1B     2B    1B     4B    1B
 */
typedef struct {
	uint8_t   sof;
	uint8_t   packet_type;
	uint16_t  data_len;
	uint8_t   cmd;
	uint32_t  crc;
	uint8_t   eof;
}__attribute__((packed)) bootloader_command_message_t;

/*
 * OTA Header format
 * __________________________________________
 * |     | Packet |     | Header |     |     |
 * | SOF | Type   | Len |  Data  | CRC | EOF |
 * |_____|________|_____|________|_____|_____|
 *   1B      1B     2B     16B     4B    1B
 */
typedef struct {
	uint8_t     sof;
	uint8_t     packet_type;
	uint16_t    data_len;
	meta_info   meta_data;
	uint32_t    crc;
	uint8_t     eof;
}__attribute__((packed)) bootloader_header_message_t;

/*
 * OTA Data format
 * __________________________________________
 * |     | Packet |     |        |     |     |
 * | SOF | Type   | Len |  Data  | CRC | EOF |
 * |_____|________|_____|________|_____|_____|
 *   1B      1B     2B    nBytes   4B    1B
 */
typedef struct {
	uint8_t     sof;
	uint8_t     packet_type;
	uint16_t    data_len;
	uint8_t     *data;
}__attribute__((packed)) bootloader_data_message_t;

/*
 * OTA Response format
 * __________________________________________
 * |     | Packet |     |        |     |     |
 * | SOF | Type   | Len | Status | CRC | EOF |
 * |_____|________|_____|________|_____|_____|
 *   1B      1B     2B      1B     4B    1B
 */
typedef struct {
	uint8_t   sof;
	uint8_t   packet_type;
	uint16_t  data_len;
	uint8_t   status;
	uint32_t  crc;
	uint8_t   eof;
}__attribute__((packed)) bootloader_response_message_t;

bootloader_exception_t bootloader_DownloadAndFlash(void);
void load_new_app( void );

#endif /* INC_BOOTLOADER_H_ */
