/*
 * bootloader.c
 *
 *  Created on: Nov 13, 2023
 *      Author: tyuga
 */

#include "bootloader.h"
#include "bsp_bootloader.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

static unsigned char rxBuffer[BOOTLOADER_PACKET_MAX_SIZE];
static bootloader_state_t otaState = BOOTLOADER_STATE_IDLE;
static unsigned int otaFwTotalSize;
static unsigned int otaFwCrc;
static unsigned int otaFwRecivedSize;
static unsigned char slotNumToWrite;
slot_cfg_t *cfg_flash  = (slot_cfg_t*)(CFG_ADDR);

static unsigned short receiveChunk(unsigned char *buf, unsigned short len);
static bootloader_exception_t processData(unsigned char *buf, unsigned short len);
static void sendResponse(unsigned char type);
static uint8_t  get_available_slot_number(void);

bootloader_exception_t bootloader_DownloadAndFlash() {
    bootloader_exception_t ret = BOOTLOADER_OK;
    unsigned short len;

    //printf("Waiting for the OTA data...\r\n");

    otaFwTotalSize = 0;
    otaFwRecivedSize = 0;
    otaFwCrc = 0;
    otaState = BOOTLOADER_STATE_START;

    do {
        memset(rxBuffer, 0, BOOTLOADER_PACKET_MAX_SIZE);

        len = receiveChunk(rxBuffer, BOOTLOADER_PACKET_MAX_SIZE);
        if (len != 0u) {
            ret = processData(rxBuffer, len);
        } else {
            ret = BOOTLOADER_ERROR;
        }

        if (ret != BOOTLOADER_OK) {
            //printf("Sending NACK\r\n");
            sendResponse(BOOTLOADER_NACK);
            break;
        } else {
            sendResponse(BOOTLOADER_ACK);
        }

    } while (otaState != BOOTLOADER_STATE_IDLE);

    return ret;
}

void load_new_app( void ) {
    slot_cfg_t cfg;
    uint32_t slot_addr;
    uint32_t crc;

    memcpy((void*)&cfg, cfg_flash, sizeof(slot_cfg_t));

    for (uint8_t i = 0; i < NUM_OF_SLOTS; i++) {
        if (cfg.slot[i].valid == 1 && cfg.slot[i].active == 1) {

            slot_addr = i == 0 ? APP_SLOT0_ADDR : APP_SLOT1_ADDR;

            if (bsp_flashProgramm(APP_ADDR, (uint8_t*)slot_addr, cfg.slot[i].size, true) != true) {
                cfg.slot[i].valid = 0;
                cfg.slot[i].active = 0;
                cfg.slot[i].size = 0;
                cfg.slot[i].crc = 0;
                bsp_flashProgramm(CFG_ADDR, (unsigned char*)&cfg, sizeof(slot_cfg_t), true);
                break;
            }

            crc = bsp_calcCRC((uint8_t*)APP_ADDR, cfg.slot[i].size);
            if (crc != cfg.slot[i].crc) {
                cfg.slot[i].valid = 0;
                cfg.slot[i].active = 0;
                cfg.slot[i].size = 0;
                cfg.slot[i].crc = 0;
                bsp_flashProgramm(CFG_ADDR, (unsigned char*)&cfg, sizeof(slot_cfg_t), true);
                break;
            }
        }
    }


}

static bootloader_exception_t processData(unsigned char *buf, unsigned short len) {
    bootloader_exception_t ret = BOOTLOADER_ERROR;

    do {
        if (buf == NULL || len == 0) {
            break;
        }

        bootloader_command_message_t *cmd_msg = (bootloader_command_message_t*)buf;
        if (cmd_msg->packet_type == BOOTLOADER_PACKET_TYPE_CMD) {
            if (cmd_msg->cmd == BOOTLOADER_CMD_ABORT) {
                break;
            }
        }

        switch (otaState) {
            case BOOTLOADER_STATE_IDLE:
                //printf("BOOTLOADER_STATE_IDLE...\r\n");
                ret = BOOTLOADER_OK;
                break;
            case BOOTLOADER_STATE_START:
                if (cmd_msg->packet_type == BOOTLOADER_PACKET_TYPE_CMD) {
                    if (cmd_msg->cmd == BOOTLOADER_CMD_START) {
                        //printf("Received OTA START Command\r\n");
                        otaState = BOOTLOADER_STATE_HEADER;
                        ret = BOOTLOADER_OK;
                    }
                }
                break;
            case BOOTLOADER_STATE_HEADER:
                bootloader_header_message_t *header_msg = (bootloader_header_message_t*)buf;
                if (header_msg->packet_type == BOOTLOADER_PACKET_TYPE_HEADER) {
                    otaFwTotalSize = header_msg->meta_data.package_size;
                    otaFwCrc = header_msg->meta_data.package_crc;
                    //printf("Received OTA header. FW size is: %d\r\n", otaFwTotalSize);
                    slotNumToWrite = get_available_slot_number();
                    if (slotNumToWrite != 0xFF) {
                        otaState = BOOTLOADER_STATE_DATA;
                        ret = BOOTLOADER_OK;
                    }
                }
                break;
            case BOOTLOADER_STATE_DATA:
                bootloader_data_message_t *data_msg = (bootloader_data_message_t*)buf;
                unsigned short len = data_msg->data_len;
                uint32_t slot_addr = slotNumToWrite == 0 ? APP_SLOT0_ADDR : APP_SLOT1_ADDR;

                if (data_msg->packet_type == BOOTLOADER_PACKET_TYPE_DATA) {
                    if (otaFwRecivedSize == 0) {
                        slot_cfg_t cfg;
                        memcpy(&cfg, cfg_flash, sizeof(slot_cfg_t));
                        cfg.slot[slotNumToWrite].valid = 1u;
                        if (bsp_flashProgramm(CFG_ADDR, (unsigned char*)&cfg, sizeof(slot_cfg_t), true) != true) {
                            ret = BOOTLOADER_ERROR;
                            break;
                        }
                    }
                    if (bsp_flashProgramm(slot_addr + otaFwRecivedSize, buf + 4, len, (otaFwRecivedSize == 0))) {
                        //printf("[%d/%d]\r\n", otaFwRecivedSize/BOOTLOADER_DATA_MAX_SIZE, otaFwTotalSize/BOOTLOADER_DATA_MAX_SIZE);
                        otaFwRecivedSize += len;
                        if (otaFwRecivedSize >= otaFwTotalSize) {
                            otaState = BOOTLOADER_STATE_END;
                        }
                        ret = BOOTLOADER_OK;
                    }
                }
                break;
            case BOOTLOADER_STATE_END:
                if (cmd_msg->packet_type == BOOTLOADER_PACKET_TYPE_CMD) {
                    if (cmd_msg->cmd == BOOTLOADER_CMD_END) {
                        //printf("Received OTA END Command\r\n");
                    	slot_cfg_t cfg;
                        uint32_t slot_addr = slotNumToWrite == 0 ? APP_SLOT0_ADDR : APP_SLOT1_ADDR;
                        uint32_t calc_crc = bsp_calcCRC((uint8_t*)slot_addr, otaFwRecivedSize);

                        if (calc_crc != otaFwCrc) {
                            break;
                        }
                        memcpy(&cfg, cfg_flash, sizeof(slot_cfg_t));
                        cfg.slot[slotNumToWrite].valid = 1;
                        cfg.slot[slotNumToWrite].active = 1;
                        cfg.slot[slotNumToWrite].size = otaFwRecivedSize;
                        cfg.slot[slotNumToWrite].crc = calc_crc;
                        for (uint8_t i = 0; i < NUM_OF_SLOTS; i++) {
                            if (i != slotNumToWrite) {
                                cfg.slot[i].active = 0;
                            }
                        }
                        if (bsp_flashProgramm(CFG_ADDR, (unsigned char*)&cfg, sizeof(slot_cfg_t), true) != true) {
                            ret = BOOTLOADER_ERROR;
                            break;
                        }

                        otaState = BOOTLOADER_STATE_IDLE;
                        ret = BOOTLOADER_OK;
                    }
                }
                break;
            default:
                ret = BOOTLOADER_ERROR;
                break;
        }
    } while (false);
    return ret;
}

static unsigned short receiveChunk(unsigned char *buf, unsigned short max_len) {
    short ret = BOOTLOADER_OK;
    unsigned short index = 0u;
    unsigned short len;
    unsigned int cal_data_crc = 0u, rec_data_crc = 0u;

    do {
        if (bsp_receiveData(&buf[index], 1) == false) {
            break;
        }
        if (buf[index++] != BOOTLOADER_SOF) {
            ret = BOOTLOADER_ERROR;
            break;
        }

        // receive type of packet
        if (bsp_receiveData(&buf[index++], 1) == false) {
            break;
        }

        // receive length of payload
        if (bsp_receiveData(&buf[index], 2) == false) {
            break;
        }
        len = *(unsigned short*)&buf[index];
        index += 2;

        // receive payload
        for (int i = 0; i < len; i++) { 
            if(bsp_receiveData(&buf[index++], 1) == false) {
                break;
            }
        }

        // receive CRC
        if (bsp_receiveData(&buf[index], 4) == false) {
            break;
        }
        rec_data_crc = *(unsigned int*)(&buf[index]);
        index += 4;

        // receive EOF
        if (bsp_receiveData(&buf[index], 1) == false) {
            break;
        }

        if (buf[index++] != BOOTLOADER_EOF) {
            ret = BOOTLOADER_ERROR;
            break;
        }

        cal_data_crc = bsp_calcCRC(&buf[4], len);
        if (cal_data_crc != rec_data_crc) {
            ret = BOOTLOADER_ERROR;
            break;
        }
    } while (false);

    if (ret != BOOTLOADER_OK) {
        index = 0;
    }

    if (max_len < index) {
        //printf("Received more data then expected. Received %d. Expected %d\r\n", max_len, index);
        index = 0;
    }
    
    return index;
}

static void sendResponse(unsigned char type) {
    bootloader_response_message_t rsp = {
        .sof = BOOTLOADER_SOF,
        .packet_type = BOOTLOADER_PACKET_TYPE_RESPONSE,
        .data_len = 1,
        .status = type,
        .crc = bsp_calcCRC(&type, 1),
        .eof = BOOTLOADER_EOF,
    };
    bsp_sendData((unsigned char*)(&rsp), sizeof(bootloader_response_message_t));
}

/**
  * @brief Return the available slot number
  * @param none
  * @retval slot number
  */
static uint8_t  get_available_slot_number() {
    uint8_t slot_number = 0xFF;
    slot_cfg_t cfg;
    memcpy(&cfg, cfg_flash, sizeof(slot_cfg_t));
    if (cfg_flash->magicNumber != MAGIC_NUMBER) {
        memset((void*)&cfg, 0, sizeof(slot_cfg_t));
        cfg.magicNumber = MAGIC_NUMBER;
        if (bsp_flashProgramm(CFG_ADDR, (unsigned char*)&cfg, sizeof(slot_cfg_t), true) != true) {
            return false;
        }
    }

    for (uint8_t i = 0; i < NUM_OF_SLOTS; i++) {
        if (cfg.slot[i].active == 0u) {
            slot_number = i;
            break;
        }
    }

    return slot_number;
}
