/*!
    \brief
    \Author  huqifan
    \Date2019-08-15 16:24:16
    \LastEditTime2019-08-28 10:19:54
*/
#ifndef EEPROM_H
#define EEPROM_H

#include "gd32e10x.h"

#define PAGE_SIZE 1024
#define FLASH_COMPLETE 0
#define VARIABLE_MAX_SIZE 64

/* 使用124、125页 */
#define EEPROM_START_ADDRESS ((uint32_t)(0x08000000 + 124 * 1024))

/* Pages 0 and 1 base and end addresses */
#define PAGE0_BASE_ADDRESS ((uint32_t)(EEPROM_START_ADDRESS + 0x000))
#define PAGE0_END_ADDRESS ((uint32_t)(EEPROM_START_ADDRESS + (PAGE_SIZE - 1)))

#define PAGE1_BASE_ADDRESS ((uint32_t)(EEPROM_START_ADDRESS + PAGE_SIZE))
#define PAGE1_END_ADDRESS \
  ((uint32_t)(EEPROM_START_ADDRESS + (2 * PAGE_SIZE - 1)))

/* Used Flash pages for EEPROM emulation */
#define PAGE0 ((uint16_t)0x0000)
#define PAGE1 ((uint16_t)0x0001)

/* No valid page define */
#define NO_VALID_PAGE ((uint16_t)0x00AB)

/* Valid pages in read and write defines */
#define READ_FROM_VALID_PAGE ((uint8_t)0x00)
#define WRITE_IN_VALID_PAGE ((uint8_t)0x01)

/* Page full define */
#define PAGE_FULL ((uint8_t)0x80)

/* 变量超出大小 */
#define VAR_SIZE_OVERFLOW  ((uint16_t)0x00AC)
#define ADDR_INVALID      ((uint16_t)0x00AD)
#define POINT_INVALID     ((uint16_t)0x00AE)
#define MARK_INVALID      ((uint16_t)0x00AF)

enum {
  IDX_START = 0xDF00,
  //
    IDX_GIMBLE_NAME,
    IDX_WB,
    IDX_IS_SLE_ISO,
    IDX_ISO_SLE,
    IDX_EC_SLE,
    IDX_INCEPTION_SPEED,
    IDX_TIME_LAPSE_PAN,
    IDX_TIME_LAPSE_TILT,
    IDX_TIME_LAPSE_INVL,
    IDX_TIME_LAPSE_DWELL,
    IDX_SCENE_SLE,
    IDX_SCENE_CUSTOM_SPEED,
    IDX_SCENE_CUSTOM_DEAD,
    IDX_KNOB_GIMBLE_SENS,
    IDX_KNOB_CAMERA_SENS,
    IDX_KNOB_OBJ,
    IDX_AUTO_FOCUS_TIME,
    IDX_LANGUAGE,
  //
  IDX_BUTT
};

/* Variables' number */
#define NumbOfVar ((uint8_t)(IDX_BUTT - IDX_START - 1))

uint16_t EE_Init(void);
uint16_t EE_ReadVariable(uint16_t virt_addr, void* data, uint16_t size, uint16_t *br);
uint16_t EE_WriteVaribal(uint16_t virt_addr, void* data, uint16_t size);

uint16_t BaseWrite(uint32_t addr, void* data, uint16_t size);
uint16_t BaseRead(uint32_t addr, void* data, uint16_t size);
uint16_t BaseErase(uint32_t addr);
uint16_t EE_Mark(uint32_t addr, uint64_t mk);

#endif
