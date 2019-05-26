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
#include <Fat16Config.h>
#include <SdCard.h>

//==============================================================================
// private helper functions
//------------------------------------------------------------------------------
static bool wait_for_token(sd_card_t *card, uint8_t token, uint16_t timeoutMillis);
static uint8_t card_acmd(sd_card_t *card, uint8_t cmd, uint32_t arg);
static uint8_t card_command(sd_card_t *card, uint8_t cmd, uint32_t arg);
static void error(sd_card_t *card, uint8_t code, uint8_t data);
static bool read_reg(sd_card_t *card, uint8_t cmd, void* buf);
static bool read_transfer(sd_card_t *card, uint8_t* dst, uint16_t count);

// Wait for card to go not busy. Return false if timeout.
static bool wait_for_token(sd_card_t *card, uint8_t token, uint16_t timeoutMillis) {
  // FIXME millis
  uint16_t t0 = millis();
  while (card->spiRecByte() != token) {
    if (((uint16_t)millis() - t0) > timeoutMillis) return false;
  }
  return true;
}

static uint8_t card_acmd(sd_card_t *card, uint8_t cmd, uint32_t arg) {
  card_command(card, CMD55, 0);
  return card_command(card, cmd, arg);
}

static uint8_t card_command(sd_card_t *card, uint8_t cmd, uint32_t arg) {
  uint8_t r1;

  // select card
  card->chipSelectLow();

  // wait if busy
  wait_for_token(card, 0XFF, SD_WRITE_TIMEOUT);

  // send command
  card->spiSendByte(cmd | 0x40);

  // send argument
  for (int8_t s = 24; s >= 0; s -= 8) card->spiSendByte(arg >> s);

  // send CRC - must send valid CRC for CMD0
  card->spiSendByte(cmd == CMD0 ? 0x95 : 0XFF);

  // wait for not busy
  for (uint8_t retry = 0; (0X80 & (r1 = card->spiRecByte())) && retry != 0XFF; retry++);
  return r1;
}

static void error(sd_card_t *card, uint8_t code, uint8_t data) {
  card->errorData = data;
  card->errorCode = code;
  card->chipSelectHigh();
}

static bool read_reg(sd_card_t *card, uint8_t cmd, void* buf) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(buf);
  if (card_command(card, cmd, 0)) {
    card->chipSelectHigh();
    return false;
  }
  return read_transfer(card, dst, 16);
}

static bool read_transfer(sd_card_t *card, uint8_t* dst, uint16_t count) {
  // wait for start of data
  if (!wait_for_token(card, DATA_START_BLOCK, SD_READ_TIMEOUT)) {
    error(card, SD_ERROR_READ_TIMEOUT, 0);
  }

  // FIXME - not agnostic
  // start first spi transfer
  SPDR = 0XFF;
  for (uint16_t i = 0; i < count; i++) {
    while (!(SPSR & (1 << SPIF)));
    dst[i] = SPDR;
    SPDR = 0XFF;
  }
  // wait for first CRC byte
  while (!(SPSR & (1 << SPIF)));
  card->spiRecByte();  // second CRC byte
  card->chipSelectHigh();
  return true;
}

//==============================================================================
// sd_card main functions
//------------------------------------------------------------------------------

/**
 * Determine the size of a standard SD flash memory card
 * \return The number of 512 byte data blocks in the card
 */
uint32_t sd_card_size(sd_card_t *card) {
  uint16_t c_size;
  csd_t csd;
  if (!read_reg(card, CMD9, &csd)) return 0;
  uint8_t read_bl_len = csd.v1.read_bl_len;
  c_size = (csd.v1.c_size_high << 10) | (csd.v1.c_size_mid << 2) | csd.v1.c_size_low;
  uint8_t c_size_mult = (csd.v1.c_size_mult_high << 1) | csd.v1.c_size_mult_low;
  return (uint32_t)(c_size+1) << (c_size_mult + read_bl_len - 7);
}
//------------------------------------------------------------------------------
/**
 * Initialize a SD flash memory card.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 *
 */
bool sd_init(sd_card_t *card) {

  card->errorCode = 0;
  uint8_t r;
  // 16-bit init start time allows over a minute
  uint16_t t0 = (uint16_t)millis();

  card->chipSelectHigh();

  // FIXME this should die
  // Enable SPI, Master, clock rate F_CPU/128
  SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);

  // must supply min of 74 clock cycles with CS high.
  for (uint8_t i = 0; i < 10; i++) card->spiSendByte(0XFF);
  card->chipSelectLow();

  // command to go idle in SPI mode
  while ((r = card_command(card, CMD0, 0)) != R1_IDLE_STATE) {
    if (((uint16_t)millis() - t0) > SD_INIT_TIMEOUT) {
      error(card, SD_ERROR_CMD0, r);
      return false;
    }
  }
#if USE_ACMD41
  // start initialization and wait for completed initialization
  while ((r = card_acmd(card, ACMD41, 0)) != R1_READY_STATE) {
    if (((uint16_t)millis() - t0) > SD_INIT_TIMEOUT) {
      error(card, SD_ERROR_ACMD41, r);
      return false;
    }  
  }
#else  // USE_ACMD41
  // use CMD1 to initialize the card - works with MMC and some SD cards
  while ((r = card_command(card, CMD1, 0)) != R1_READY_STATE) {
    if (((uint16_t)millis() - t0) > SD_INIT_TIMEOUT) {
      error(card, SD_ERROR_CMD1, r);
      return false;
    }   
  }
#endif  // USE_ACMD41
  card->chipSelectHigh();
  return true;
}

bool sd_read_cid(sd_card_t *card, cid_t *cid) {
    return read_reg(card, CMD10, cid);
}

//------------------------------------------------------------------------------
/**
 * Reads a 512 byte block from a storage device.
 *
 * \param[in] blockNumber Logical block to be read.
 * \param[out] dst Pointer to the location that will receive the data.
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 */
bool sd_read_block(sd_card_t *card, uint32_t blockNumber, uint8_t* dst) {
  if (card_command(card, CMD17, blockNumber << 9)) {
    error(card, SD_ERROR_CMD17, 0);
    return false;
  }
  return read_transfer(card, dst, 512);
}
//------------------------------------------------------------------------------
/**
 * Writes a 512 byte block to a storage device.
 *
 * \param[in] blockNumber Logical block to be written.
 * \param[in] src Pointer to the location of the data to be written.
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 */
bool sd_write_block(sd_card_t *card, uint32_t blockNumber, const uint8_t* src) {
  uint32_t address = blockNumber << 9;

  if (card_command(card, CMD24, address)) {
    error(card, SD_ERROR_CMD24, 0);
    return false;
  }

  // FIXME
  // optimize write loop
  SPDR = DATA_START_BLOCK;
  for (uint16_t i = 0; i < 512; i++) {
    while (!(SPSR & (1 << SPIF)));
    SPDR = src[i];
  }
  while (!(SPSR & (1 << SPIF)));  // wait for last data byte
  card->spiSendByte(0xFF);  // dummy crc
  card->spiSendByte(0xFF);  // dummy crc

  // get write response
  uint8_t r1 = card->spiRecByte();
  if ((r1 & DATA_RES_MASK) != DATA_RES_ACCEPTED) {
    error(card, SD_ERROR_WRITE_RESPONSE, r1);
    return false;
  }
  // wait for card to complete write programming
  if (!wait_for_token(card, 0XFF, SD_WRITE_TIMEOUT)) {
      error(card, SD_ERROR_WRITE_TIMEOUT, 0);
  }
  card->chipSelectHigh();
  return true;
}
