/* Arduino FAT16 Library
 * Copyright (C) 2008 by William Greiman
 *
 * This file is part of the Arduino FAT16 Library
 *
 * This Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with the Arduino Fat16 Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef SdCard_h
#define SdCard_h
 /**
  * \file
  * SdCard class
  */
#include <Arduino.h>
#include <SdInfo.h>
//------------------------------------------------------------------------------
// error codes
/** Card did not go into SPI mode */
uint8_t const SD_ERROR_CMD0              = 0X1;

/** Card did not go ready */
uint8_t const SD_ERROR_ACMD41            = 0X2;

/** Write command not accepted */
uint8_t const SD_ERROR_CMD24             = 0X3;

/** Read command not accepted */
uint8_t const SD_ERROR_CMD17             = 0X4;

/** timeout waiting for read data */
uint8_t const SD_ERROR_READ_TIMEOUT      = 0X5;

/** write error occurred */
uint8_t const SD_ERROR_WRITE_RESPONSE    = 0X6;

/** timeout waiting for write status */
uint8_t const SD_ERROR_WRITE_TIMEOUT     = 0X7;

/** attempt to write block zero */
uint8_t const SD_ERROR_BLOCK_ZERO_WRITE  = 0X8;

/** card returned an error to a CMD13 status check after a write */
uint8_t const SD_ERROR_WRITE_PROGRAMMING = 0X9;

/** card fialed to initialize with CMD1*/
uint8_t const SD_ERROR_CMD1              = 0XA;

//------------------------------------------------------------------------------
/**
 * \class SdCard
 * \brief Hardware access class for SD flash cards
 *
 * Supports raw access to a standard SD flash memory card.
 *
 */

typedef struct sd_card {
  // SPI function pointers. This library now assumes that the caller will
  // provide a fully initialized SPI module with their own speed.
  // Function pointers need to be passed in to here.
  void (*spiSendByte)(uint8_t);
  uint8_t (*spiRecByte)(void);
  void (*chipSelectHigh)(void);
  void (*chipSelectLow)(void);

  // Code for a SD error. See SdCard.h for definitions.
  uint8_t errorCode;

  // Data that may be helpful in determining the cause of an error
  uint8_t errorData;
  

} sd_card_t;

bool sd_init(sd_card_t *card);
uint32_t sd_card_size(sd_card_t *card);

/** 
* Read the CID register which contains info about the card.
* This includes Manufacturer ID, OEM ID, product name, version,
* serial number, and manufacturing date.
*
* \param[out] cid location for CID data.
*
* \return The value one, true, is returned for success and
*         the value zero, false, is returned for failure.
*/
bool sd_read_cid(sd_card_t *card, cid_t *cid);

bool sd_read_block(sd_card_t *card, uint32_t block, uint8_t* dst);
bool sd_write_block(sd_card_t *card, uint32_t block, const uint8_t* src);

#endif  // SdCard_h
