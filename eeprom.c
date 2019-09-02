/*!
    \brief
    \Author  huqifan
    \Date2019-08-15 16:24:26
    \LastEditTime2019-08-28 09:33:29
*/
#include "eeprom.h"

/* 虚拟地址数组(0xFFFF不允许) */
extern uint16_t virt_addr_var_tab[NumbOfVar];

/* 内部全局变量，用于保存读出的变量 */
uint8_t data_var[VARIABLE_MAX_SIZE];

/*  Page status definitions
  在FLASH中的样子(低字节在前)：
  ERASED              FFFF FFFF FFFF FFFF
  RECEIVE_DATA        FFFF FFFF EEEE EEEE
  VALID_PAGE          0000 0000 EEEE EEEE
 */
const uint64_t ERASED = ((uint64_t)0xFFFFFFFFFFFFFFFF);
const uint64_t RECEIVE_DATA = ((uint64_t)0xEEEEEEEEFFFFFFFF);
const uint64_t VALID_PAGE = ((uint64_t)0xEEEEEEEE00000000);

static uint16_t EE_Format(void);
static uint16_t EE_FindValidPage(uint8_t Operation);
static uint16_t EE_VerifyPageFullWriteVariable(uint16_t virt_addr, void* data,
                                               uint16_t size);
static uint16_t EE_PageTransfer(uint16_t virt_addr, void* data, uint16_t size);
static uint16_t EE_FindFirstVirAddr(uint16_t page);

/*!
     PAGE0     |    PAGE1     |   操作
  -------------+--------------+----------------------------------
  ERASED       | ERASED       | 无效状态，擦除两个页并格式化PAGE0
  ERASED       | RECEIVE_DATA | 擦除PAGE0，将PAGE1标记为VALID_PAGE
  ERASED       | VALID_PAGE   | 将PAGE1作为有效页使用，擦除PAGE0
  -------------+--------------+----------------------------------
  RECEIVE_DATA | ERASED       | 擦除PAGE1，将PAGE0标记为VALID_PAGE
  RECEIVE_DATA | RECEIVE_DATA | 无效状态，擦除两个页并格式化PAGE0
  RECEIVE_DATA | VALID_PAGE   |
  将PAGE1作为有效页使用，将PAGE1变量传输至PAGE0，标记PAGE0为有效，擦除PAGE1
  -------------+--------------+----------------------------------
  VALID_PAGE   | ERASED       | 将PAGE0作为有效页使用，擦除PAGE1
  VALID_PAGE   | RECEIVE_DATA |
  将PAGE0作为有效页使用，将PAGE0变量传输至PAGE1，标记PAGE1为有效，擦除PAGE0
  VALID_PAGE   | VALID_PAGE   | 无效状态，擦除两个页并格式化PAGE0

    \brief      EEPROM 初始化
    \param[in]  none
    \param[out] none
    \retval     状态
      \arg        FLASH_COMPLETE: 成功
      \arg        其他: 失败
*/
uint16_t EE_Init(void) {
  uint64_t page0_status = 0;
  uint64_t page1_status = 0;
  uint32_t flash_status = 0;
  uint16_t x;
  uint16_t read_status = 0;
  uint16_t eeprom_status;
  uint16_t byte_read;
  uint16_t var_idx;

  BaseRead(PAGE0_BASE_ADDRESS, &page0_status, sizeof(page0_status));
  BaseRead(PAGE1_BASE_ADDRESS, &page1_status, sizeof(page1_status));

  if (page0_status == ERASED) {
    if (page1_status == VALID_PAGE) {
      /* 将PAGE1作为有效页使用，擦除PAGE0 */
      flash_status = BaseErase(PAGE0_BASE_ADDRESS);
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }
    } else if (page1_status == RECEIVE_DATA) {
      /* 擦除PAGE0，将PAGE1标记为VALID_PAGE */
      flash_status = BaseErase(PAGE0_BASE_ADDRESS);
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }

      flash_status = EE_Mark(PAGE1_BASE_ADDRESS, VALID_PAGE);
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }
    } else {
      /* 无效状态，擦除两个页并格式化PAGE0 */
      flash_status = EE_Format();
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }
    }
  } else if (page0_status == RECEIVE_DATA) {
    if (page1_status == VALID_PAGE) {
      /* 将PAGE1作为有效页使用，将PAGE1变量传输至PAGE0，标记PAGE0为有效，擦除PAGE1
       */
      x = EE_FindFirstVirAddr(PAGE0);
      for (var_idx = 0; var_idx < NumbOfVar; var_idx++) {
        if (x == virt_addr_var_tab[var_idx]) {
          continue;
        }
        read_status = EE_ReadVariable(virt_addr_var_tab[var_idx], &data_var,
                                      VARIABLE_MAX_SIZE, &byte_read);
        if (read_status != 0x1) {
          eeprom_status = EE_VerifyPageFullWriteVariable(
              virt_addr_var_tab[var_idx], data_var, byte_read);
          if (eeprom_status != FLASH_COMPLETE) {
            return eeprom_status;
          }
        }
      }
      flash_status = EE_Mark(PAGE0_BASE_ADDRESS, VALID_PAGE);
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }

      flash_status = BaseErase(PAGE1_BASE_ADDRESS);
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }
    } else if (page1_status == ERASED) {
      /* 擦除PAGE1，将PAGE0标记为VALID_PAGE */
      flash_status = BaseErase(PAGE1_BASE_ADDRESS);
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }

      flash_status = EE_Mark(PAGE0_BASE_ADDRESS, VALID_PAGE);
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }
    } else {
      /* 无效状态，擦除两个页并格式化PAGE0 */
      flash_status = EE_Format();
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }
    }
  }

  else if (page0_status == VALID_PAGE) {
    if (page1_status == VALID_PAGE) {
      /* 无效状态，擦除两个页并格式化PAGE0 */
      flash_status = EE_Format();
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }
    } else if (page1_status == ERASED) {
      /* 将PAGE0作为有效页使用，擦除PAGE1 */
      flash_status = BaseErase(PAGE1_BASE_ADDRESS);
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }
    } else {
      /* 将PAGE0作为有效页使用，将PAGE0变量传输至PAGE1，标记PAGE1为有效，擦除PAGE0
       */
      x = EE_FindFirstVirAddr(PAGE1);
      for (var_idx = 0; var_idx < NumbOfVar; var_idx++) {
        if (x == virt_addr_var_tab[var_idx]) {
          continue;
        }
        read_status = EE_ReadVariable(virt_addr_var_tab[var_idx], &data_var,
                                      VARIABLE_MAX_SIZE, &byte_read);
        if (read_status != 0x1) {
          eeprom_status = EE_VerifyPageFullWriteVariable(
              virt_addr_var_tab[var_idx], data_var, byte_read);
          if (eeprom_status != FLASH_COMPLETE) {
            return eeprom_status;
          }
        }
      }
      flash_status = EE_Mark(PAGE1_BASE_ADDRESS, VALID_PAGE);
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }
      flash_status = BaseErase(PAGE0_BASE_ADDRESS);
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }
    }
  } else {
    /* 无效状态，擦除两个页并格式化PAGE0 */
    flash_status = EE_Format();
    if (flash_status != FLASH_COMPLETE) {
      return flash_status;
    }
  }

  return 0;
}

/*!
    \brief      读取存储的变量
    \param[in]  virt_addr: 虚拟地址
    \param[in]  size: 要读取的大小
    \param[out] data: 接收读出数据的缓冲区
    \param[out] br: 实际读取到的字节数，NULL 时不使用
    \retval       读取状态
      \arg        0: 成功查找到要读取的变量
      \arg        1: 未查找到要读取的变量
      \arg        NO_VALID_PAGE: 未查找到valid页
*/
uint16_t EE_ReadVariable(uint16_t virt_addr, void* data, uint16_t size,
                         uint16_t* br) {
  uint16_t valid_page;
  uint16_t read_status = 1;
  uint32_t read_addr, page_start_addr;
  uint16_t addr_value; /* 16byte，FLASH 中存储的虚拟地址值 */
  uint16_t store_len;  /* 16byte，FLASH 中存储的虚拟地址对应长度 */
  uint16_t store_word_len;

  valid_page = EE_FindValidPage(READ_FROM_VALID_PAGE);
  if (valid_page == NO_VALID_PAGE) {
    return NO_VALID_PAGE;
  }

  page_start_addr =
      (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(valid_page * PAGE_SIZE));

  read_addr = (uint32_t)((EEPROM_START_ADDRESS - 4) +
                         (uint32_t)((1 + valid_page) * PAGE_SIZE));

  /* 从后往前查找 */
  while (read_addr > (page_start_addr + 8)) {
    addr_value = *(__IO uint16_t*)(read_addr + 2);
    store_len = *(__IO uint16_t*)read_addr;
    if (store_len != 0xFFFF) {
      store_word_len = store_len / 4;
      if (store_len % 4 != 0) {
        store_word_len += 1;
      }
    }

    if (addr_value == virt_addr) {
      size = size > store_len ? store_len : size;
      if (br != (void*)0) {
        *br = size;
      }
      BaseRead(read_addr - (4 * store_word_len), data, size);
      read_status = 0;
      break;
    } else if (addr_value != 0xFFFF && store_len != 0xFFFF) {
      read_addr -= (4 * store_word_len + 4);
    } else {
      read_addr -= 4;
    }
  }

  /* Return read_status value: (0: variable exist, 1: variable doesn't exist) */
  return read_status;
}

/*!
    \brief      写入变量
    \param[in]  virt_addr: 虚拟地址
    \param[in]  data: 要存储的数据缓冲区地址
    \param[in]  size: 要存储的数据大小
    \param[out] none
    \retval
      \arg        FLASH_COMPLETE: 成功写入
      \arg        PAGE_FULL: 页满
      \arg        NO_VALID_PAGE: 未查找到可用页
      \arg        VAR_SIZE_OVERFLOW: 长度超过设定
      \arg        Flash error code: on write Flash error
*/
uint16_t EE_WriteVaribal(uint16_t virt_addr, void* data, uint16_t size) {
  if (size > VARIABLE_MAX_SIZE) {
    return VAR_SIZE_OVERFLOW;
  }
  uint16_t status = EE_VerifyPageFullWriteVariable(virt_addr, data, size);
  if (status == PAGE_FULL) {
    status = EE_PageTransfer(virt_addr, data, size);
  }
  return status;
}

/*!
    \brief      擦除 PAGE0 和 PAGE1，写 VALID_PAGE 至 PAGE0
    \param[in]  none
    \param[out] none
    \retval     Flash 操作状态
      \arg        FLASH_COMPLETE: 成功
      \arg        其他: 错误码
*/
static uint16_t EE_Format(void) {
  uint16_t flash_status = BaseErase(PAGE0_BASE_ADDRESS);
  if (flash_status != FLASH_COMPLETE) {
    return flash_status;
  }

  flash_status = EE_Mark(PAGE0_BASE_ADDRESS, VALID_PAGE);
  if (flash_status != FLASH_COMPLETE) {
    return flash_status;
  }

  flash_status = BaseErase(PAGE1_BASE_ADDRESS);
  if (flash_status != FLASH_COMPLETE) {
    return flash_status;
  }

  return FLASH_COMPLETE;
}

/*!
    \brief      为写或读操作查找 VALID_PAGE
    \param[in]  Operation: 操作
      \arg        READ_FROM_VALID_PAGE: 从 VALIDE_PAGE 读
      \arg        WRITE_IN_VALID_PAGE: 从 VALIDE_PAGE 写
    \param[out] none
    \retval     页编码
      \arg        PAGE0
      \arg        PAGE1
      \arg        NO_VALID_PAGE
*/
static uint16_t EE_FindValidPage(uint8_t Operation) {
  uint64_t page0_status = 0;
  uint64_t page1_status = 0;

  BaseRead(PAGE0_BASE_ADDRESS, &page0_status, sizeof(page0_status));
  BaseRead(PAGE1_BASE_ADDRESS, &page1_status, sizeof(page1_status));

  switch (Operation) {
    case WRITE_IN_VALID_PAGE:
      if (page1_status == VALID_PAGE) {
        if (page0_status == RECEIVE_DATA) {
          return PAGE0;
        } else {
          return PAGE1;
        }
      } else if (page0_status == VALID_PAGE) {
        if (page1_status == RECEIVE_DATA) {
          return PAGE1;
        } else {
          return PAGE0;
        }
      } else {
        return NO_VALID_PAGE;
      }
      break;
    case READ_FROM_VALID_PAGE:
      if (page0_status == VALID_PAGE) {
        return PAGE0;
      } else if (page1_status == VALID_PAGE) {
        return PAGE1;
      } else {
        return NO_VALID_PAGE;
      }
      break;

    default:
      return PAGE0;
  }
}

/*!
   \brief      检查 VALID_PAGE 是否满，在 EEPROM 写入变量
   \param[in]  virt_addr: 16 bit virtual address of the variable
   \param[in]  size:
   \param[in]  data:
   \param[out] none
   \retval     成功或错误状态:
     \arg        FLASH_COMPLETE: 成功
     \arg        PAGE_FULL: VALID_PAGE 满
     \arg        NO_VALID_PAGE: 没有 VALID_PAGE
     \arg        Flash error code: 写 Flash 错误码
*/
static uint16_t EE_VerifyPageFullWriteVariable(uint16_t virt_addr, void* data,
                                               uint16_t size) {
  if (size == 0) {
    return FLASH_COMPLETE;
  }
  uint16_t flash_status = FLASH_COMPLETE;
  uint16_t valid_page = PAGE0;
  uint32_t write_addr, page_start_addr, page_end_addr;
  uint32_t word_size = 0;

  valid_page = EE_FindValidPage(WRITE_IN_VALID_PAGE);

  if (valid_page == NO_VALID_PAGE) {
    return NO_VALID_PAGE;
  }

  page_start_addr =
      (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(valid_page * PAGE_SIZE));
  page_end_addr = (uint32_t)(EEPROM_START_ADDRESS +
                             (uint32_t)((1 + valid_page) * PAGE_SIZE));
  write_addr = (uint32_t)((EEPROM_START_ADDRESS - 4) +
                          (uint32_t)((1 + valid_page) * PAGE_SIZE));

  /* 从后往前查找第一个不是 0xFFFFFFFF 的地址，在其后写入 */
  while (write_addr >= (page_start_addr + 4)) {
    if (*(__IO uint32_t*)write_addr != 0xFFFFFFFF) {
      write_addr += 4;
      if (page_end_addr - write_addr >= (size + 4)) {
        /* 写入变量 */
        flash_status = BaseWrite(write_addr, data, size);
        if (flash_status != FLASH_COMPLETE) {
          return flash_status;
        }
        /* 写入虚拟地址和变量大小 */
        uint32_t temp_data = (virt_addr << 16) + size;
        word_size = size / 4;
        if (size % 4 != 0) {
          word_size += 1;
        }
        flash_status = BaseWrite(write_addr + word_size * 4, &temp_data,
                                 sizeof(temp_data));
        return flash_status;
      } else {
        return PAGE_FULL;
      }
    } else {
      write_addr -= 4;
    }
  }

  return PAGE_FULL;
}

/*!
   \brief      将最新的数据从已满页传输到另一页
   \param[in]  virt_addr: 新写入的变量虚拟地址
   \param[in]  size: 字节数
   \param[in]  data: 数据地址
   \param[out] none
   \retval     成功或错误状态:
     \arg        FLASH_COMPLETE: 成功
     \arg        PAGE_FULL: 页满
     \arg        NO_VALID_PAGE: 没有找到可用页
     \arg        Flash error code: 写Flash的错误码
*/
static uint16_t EE_PageTransfer(uint16_t virt_addr, void* data, uint16_t size) {
  uint16_t flash_status;
  uint32_t new_page_addr, old_page_addr;
  uint16_t valid_page, var_idx;
  uint16_t eeprom_status, read_status;
  uint16_t byte_read;

  valid_page = EE_FindValidPage(READ_FROM_VALID_PAGE);

  if (valid_page == PAGE1) {
    new_page_addr = PAGE0_BASE_ADDRESS;
    old_page_addr = PAGE1_BASE_ADDRESS;
  } else if (valid_page == PAGE0) {
    new_page_addr = PAGE1_BASE_ADDRESS;
    old_page_addr = PAGE0_BASE_ADDRESS;
  } else {
    return NO_VALID_PAGE;
  }

  flash_status = EE_Mark(new_page_addr, RECEIVE_DATA);
  if (flash_status != FLASH_COMPLETE) {
    return flash_status;
  }

  eeprom_status = EE_VerifyPageFullWriteVariable(virt_addr, data, size);
  if (eeprom_status != FLASH_COMPLETE) {
    return eeprom_status;
  }
  /* 查找表中的每个虚拟地址 */
  for (var_idx = 0; var_idx < NumbOfVar; var_idx++) {
    /* 跳过新写入的虚拟地址 */
    if (virt_addr_var_tab[var_idx] != virt_addr) {
      read_status = EE_ReadVariable(virt_addr_var_tab[var_idx], &data_var,
                                    VARIABLE_MAX_SIZE, &byte_read);
      if (read_status != 0x1) {
        eeprom_status = EE_VerifyPageFullWriteVariable(
            virt_addr_var_tab[var_idx], data_var, byte_read);
        if (eeprom_status != FLASH_COMPLETE) {
          return eeprom_status;
        }
      }
    }
  }
  /* 擦除旧页，标记为 ERASED */
  flash_status = BaseErase(old_page_addr);
  if (flash_status != FLASH_COMPLETE) {
    return flash_status;
  }
  /* 标记新页为 VALID_PAGE */
  flash_status = EE_Mark(new_page_addr, VALID_PAGE);
  if (flash_status != FLASH_COMPLETE) {
    return flash_status;
  }

  return flash_status;
}

/*!
    \brief      写指定标记到指定页数
    \param[in]  addr: 绝对地址
    \param[in]  mk:
      \arg        ERASED: FFFF FFFF FFFF FFFF
      \arg        RECEIVE_DATA: EEEE EEEE FFFF FFFF
      \arg        VALID_PAGE: EEEE EEEE 0000 0000
    \param[out] none
    \retval     FLASH状态
      \arg        FLASH_COMPLETE: 成功
      \arg        其他: 错误码
*/
uint16_t EE_Mark(uint32_t addr, uint64_t mk) {
  uint32_t temp;
  uint16_t flash_status;
  if (mk == ERASED) { /* 擦除状态 */
    return FLASH_COMPLETE;
  } else if (mk == RECEIVE_DATA) { /* 接收状态 */
    temp = 0xEEEEEEEE;
    return BaseWrite(addr + 4, &temp, sizeof(temp));
  } else if (mk == VALID_PAGE) { /* 可用状态 */
    /* 读出页第二个字的数据 */
    BaseRead(addr + 4, &temp, sizeof(temp));
    /* 如果4个字节全是F，则将其全写为E */
    if (temp == 0xFFFFFFFF) {
      temp = 0xEEEEEEEE;
      flash_status = BaseWrite(addr + 4, &temp, sizeof(temp));
      if (flash_status != FLASH_COMPLETE) {
        return flash_status;
      }
    }
    /* 将页首四个字节写为全0 */
    temp = (uint32_t)0;
    return BaseWrite(addr, &temp, sizeof(temp));
  }
  return MARK_INVALID;
}

/*!
    \brief      查找指定页存储的第一个虚拟地址并返回
    \param[in]  page: 页编号
      \arg        PAGE0
      \arg        PAGE1
    \param[out] none
    \retval     第一个虚拟地址
      \arg        非0xFFFF: 虚拟地址
      \arg        0xFFFF: 参数错误
*/
static uint16_t EE_FindFirstVirAddr(uint16_t page) {
  if (page != PAGE0 && page != PAGE1) {
    return 0xFFFF;
  }
  uint32_t read_addr, page_start_addr;
  uint16_t addr_value; /* 16byte，FLASH 中存储的虚拟地址值 */
  uint16_t store_len;  /* 16byte，FLASH 中存储的虚拟地址对应长度 */
  uint16_t store_word_len;

  page_start_addr =
      (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(page * PAGE_SIZE));

  read_addr = (uint32_t)((EEPROM_START_ADDRESS - 4) +
                         (uint32_t)((1 + page) * PAGE_SIZE));

  /* 从后往前查找 */
  while (read_addr > (page_start_addr + 8)) {
    addr_value = *(__IO uint16_t*)(read_addr + 2);
    store_len = *(__IO uint16_t*)read_addr;
    if (store_len != 0xFFFF) {
      store_word_len = store_len / 4;
      if (store_len % 4 != 0) {
        store_word_len += 1;
      }
    }

    if (addr_value != 0xFFFF && store_len != 0xFFFF) {
      read_addr -= (4 * store_word_len + 4);
    } else {
      read_addr -= 4;
    }
  }

  return addr_value;
}

/*!
    \brief      在指定地址写入指定字节数的数据（不足4字节按4字节写入）
    \param[in]  addr: 地址
    \param[in]  data: 数据缓冲区
    \param[in]  size: 字节数
    \param[out] none
    \retval     写入状态
      \arg      FLASH_COMPLETE: 成功
      \arg      POINT_INVALID: 写指针空
      \arg      其他: 错误码
*/
uint16_t BaseWrite(uint32_t addr, void* data, uint16_t size) {
  if (addr < PAGE0_BASE_ADDRESS || (addr + size) > PAGE1_END_ADDRESS) {
    return ADDR_INVALID;
  }
  if (data == (void*)0) {
    return POINT_INVALID;
  }
  if (size == 0) {
    return FLASH_COMPLETE;
  }
  uint8_t* p_data = (uint8_t*)data;
  uint32_t remain_data = 0;
  uint16_t word_size = size / 4;
  uint32_t data_temp;

  /* 按字（4byte）写入 */
  while (word_size--) {
    data_temp = *(uint32_t*)p_data;
    fmc_word_program(addr, data_temp);
    addr += 4;
    p_data += 4;
  }

  size %= 4;
  for (uint16_t i = 0; i < size; i++) {
    remain_data += (*p_data++) << (i * 8);
  }
  if (size != 0) {
    fmc_word_program(addr, remain_data);
  }

  return FLASH_COMPLETE;
}

/*!
    \brief      读取指定地址的指定字节数据
    \param[in]  addr: 起始地址
    \param[in]  size: 读取字节数
    \param[out] data: 接收数据缓冲区
    \retval     读取状态
      \arg        FLASH_COMPLETE: 成功
      \arg        ADDR_INVALID: 地址不在EEPROM范围
      \arg        POINT_INVALID: 接收指针空
*/
uint16_t BaseRead(uint32_t addr, void* data, uint16_t size) {
  if (addr < PAGE0_BASE_ADDRESS || (addr + size) > PAGE1_END_ADDRESS) {
    return ADDR_INVALID;
  }
  if (data == (void*)0) {
    return POINT_INVALID;
  }
  uint8_t* p_data = (uint8_t*)data;
  /* 按字节（1byte）读取 */
  while (size--) {
    *p_data++ = *(__IO uint8_t*)addr++;
  }
  return FLASH_COMPLETE;
}

/*!
    \brief      擦除指定地址页
    \param[in]  addr: 页起始地址
    \param[out] none
    \retval     擦除状态
      \arg        FLASH_COMPLETE: 擦除完成
      \arg        其他: 错误码
*/
uint16_t BaseErase(uint32_t addr) {
  if (addr < PAGE0_BASE_ADDRESS || addr > PAGE1_END_ADDRESS) {
    return ADDR_INVALID;
  }
  uint32_t temp = 0;
  if (addr % PAGE_SIZE == 0) {
    for (int32_t i = 0; i < PAGE_SIZE / 4; i++) {
      BaseRead(addr + i * 4, &temp, 4);
      if (temp != 0xFFFFFFFF) {
        return fmc_page_erase(addr);
      }
    }
    /* 不需要擦除 */
    return FLASH_COMPLETE;
  }
  return fmc_page_erase(addr);
}
