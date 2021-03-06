#include <Fat16.h>
#include <Fat16Config.h>
#include <Fat16mainpage.h>
#include <SdCard.h>
#include <SdInfo.h>
#include <Fat16util.h>


// A simple data logger for the analog pins
#define CHIP_SELECT     SS // SD chip select pin
#define LOG_INTERVAL  1000 // mills between entries
#define SENSOR_COUNT     3 // number of analog pins to log
#define ECHO_TO_SERIAL   1 // echo data to serial port
#define WAIT_TO_START    1 // Wait for serial input in setup()
#define SYNC_INTERVAL 1000 // mills between calls to sync()

#include <Fat16.h>
#include <Fat16util.h> // use functions to print strings from flash memory

sd_card_t card;
Fat16 file;
uint32_t syncTime = 0;     // time of last sync()
uint32_t logTime  = 0;     // time data was logged

//------------------------------------------------------------------------------
// SPI static functions
//
// clock byte in
static uint8_t spiRec(void) {
  SPDR = 0xff;
  while (!(SPSR & (1 << SPIF)));
  return SPDR;
}
//------------------------------------------------------------------------------
// clock byte out
static void spiSend(uint8_t b) {
  SPDR = b;
  while (!(SPSR & (1 << SPIF)));
}
//------------------------------------------------------------------------------
static void csHigh(void) {
  digitalWrite(CHIP_SELECT, HIGH);
}
//------------------------------------------------------------------------------
static void csLow(void) {
  uint8_t r = 0;

  for (uint8_t b = 2; SPI_FULL_SPEED > b && r < 6; b <<= 1, r++);
  // See avr processor documentation
  SPCR = (1 << SPE) | (1 << MSTR) | (r >> 1);
  SPSR = r & 1 || r == 6 ? 0 : 1 << SPI2X;
  digitalWrite(CHIP_SELECT, LOW);
}

// store error strings in flash to save RAM
#define error(s) error_P(PSTR(s))
//------------------------------------------------------------------------------
void error_P(const char* str) {
  PgmPrint("error: ");
  SerialPrintln_P(str);
  if (card.errorCode) {
    PgmPrint("SD error: ");
    Serial.println(card.errorCode, HEX);
  }
  while(1);
}
//------------------------------------------------------------------------------
void setup(void) {
  Serial.begin(9600);
  Serial.println();
  
#if WAIT_TO_START
  PgmPrintln("Type any character to start");
  while (!Serial.available());
#endif //WAIT_TO_START

  card.spiSendByte = spiSend;
  card.spiRecByte = spiRec;
  card.chipSelectHigh = csHigh;
  card.chipSelectLow = csLow;
  card.millis = millis;

  pinMode(CHIP_SELECT, OUTPUT);

  // initialize the SD card
  if (!sd_init(&card)) error("card.begin");
  
  // initialize a FAT16 volume
  if (!Fat16::init(&card)) error("Fat16::init");
  
  // create a new file
  char name[] = "LOGGER00.CSV";
  for (uint8_t i = 0; i < 100; i++) {
    name[6] = i/10 + '0';
    name[7] = i%10 + '0';
    // O_CREAT - create the file if it does not exist
    // O_EXCL - fail if the file exists
    // O_WRITE - open for write only
    if (file.open(name, O_CREAT | O_EXCL | O_WRITE))break;
  }
  if (!file.isOpen()) error ("create");
  PgmPrint("Logging to: ");
  Serial.println(name);

  // write data header
  
  // clear write error
  file.writeError = false;
  file.print("millis");
#if ECHO_TO_SERIAL 
  Serial.print("millis");
#endif //ECHO_TO_SERIAL

#if SENSOR_COUNT > 6
#error SENSOR_COUNT too large
#endif //SENSOR_COUNT

  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    file.print(",sens");
    file.print(i, DEC);    
#if ECHO_TO_SERIAL
    Serial.print(",sens");
    Serial.print(i, DEC);
#endif //ECHO_TO_SERIAL
  }
  file.println();  
#if ECHO_TO_SERIAL
  Serial.println();
#endif  //ECHO_TO_SERIAL

  if (file.writeError || !file.sync()) {
    error("write header");
  }
}
//------------------------------------------------------------------------------
void loop() {   // run over and over again
  uint32_t m = logTime;
  // wait till time is an exact multiple of LOG_INTERVAL
  do {
      logTime = millis();
  } while (m == logTime || logTime % LOG_INTERVAL);
  // log time to file
  file.print(logTime);  
#if ECHO_TO_SERIAL
  Serial.print(logTime);
#endif //ECHO_TO_SERIAL
      
  // add sensor data 
  for (uint8_t ia = 0; ia < SENSOR_COUNT; ia++) {
    uint16_t data = analogRead(ia);
    file.write(',');
    file.print(data);
#if ECHO_TO_SERIAL
    Serial.write(',');
    Serial.print(data);
#endif //ECHO_TO_SERIAL
  }
  file.println();  
#if ECHO_TO_SERIAL
  Serial.println();
#endif //ECHO_TO_SERIAL

  if (file.writeError) error("write data");
  
  //don't sync too often - requires 2048 bytes of I/O to SD card
  if ((millis() - syncTime) <  SYNC_INTERVAL) return;
  syncTime = millis();
  if (!file.sync()) error("sync");
}
