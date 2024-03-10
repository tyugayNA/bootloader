/**
 * @file bsp_bootloader.h
 * 
 * Interface for implementation in diffrent platforms
 * 
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef INC_BSP_BOOTLOADER_H_
#define INC_BSP_BOOTLOADER_H_

#include <stdint.h>
#include <stdbool.h>

/* define address of memory */
// #define CFG_ADDR		        (0x08020000)
// #define APP_ADDR	            (0x08040000)
// #define APP_SLOT0_ADDR			(0x08060000)
// #define APP_SLOT1_ADDR			(0x08080000)

 #define NUM_OF_SLOTS			(2)
 #define SLOT_SIZE				(128 * 1024)

bool bsp_flashProgramm(unsigned int addr, unsigned char *data, unsigned short len, bool isFirstBlock);
bool bsp_receiveData(unsigned char *buf, unsigned int len);
bool bsp_sendData(unsigned char *buf, unsigned int len);
uint32_t bsp_calcCRC(uint8_t *buf, unsigned int len);

#endif /* INC_BSP_BOOTLOADER_H_ */
