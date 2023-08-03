#include "Arduino.h"
#include "SPI.h"

// ------------------------------------
// Programmer parameters

#define HW_VER 2
#define SW_MAJOR 1
#define SW_MINOR 18

// ------------------------------------
// STK opcodes

#define STK_OK 0x10
#define STK_FAILED 0x11
#define STK_UNKNOWN 0x12
#define STK_INSYNC 0x14
#define STK_NOSYNC 0x15
#define CRC_EOP 0x20
#define STK_GET_SYNC 0x30
#define STK_GET_SIGN_ON 0x31
#define STK_GET_PARAMETER 0x41
#define STK_SET_DEVICE 0x42
#define SET_DEVICE_EXT 0x45
#define STK_ENTER_PROGMODE 0x50
#define STK_LEAVE_PROGMODE 0x51
#define STK_LOAD_ADDRESS 0x55
#define STK_UNIVERSAL 0x56
#define STK_PROG_FLASH 0x60
#define STK_PROG_DATA 0x61
#define STK_PROG_PAGE 0x64
#define STK_READ_PAGE 0x74
#define STK_READ_SIGN 0x75

// ------------------------------------
// Serial config (between programmer and PC)

// Configure the serial baud rate
#define BAUDRATE 19200

// Block until there's something to read
uint8_t get_serial_char() {
  while (!Serial.available()) {
    ;
  }
  return Serial.read();
}

// ------------------------------------
// SPI and RESET config (between programmer and target)

// SPI clock pulse must be > 2 CPU cycles.
// So, take 3 cycles (i.e. divide target f_cpu by 6).
// A clock slow enough for an ATtiny85 @ 1 MHz, is a reasonable default:

#define SPI_CLOCK (1000000 / 6)

// Configure which pins to communicate with the target board via SPI
#define PIN_MOSI MOSI
#define PIN_MISO MISO
#define PIN_SCK SCK

// Write 4 bytes to the target device, while reading one after the last write
uint8_t spi_transaction(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  SPI.transfer(a);
  SPI.transfer(b);
  SPI.transfer(c);
  return SPI.transfer(d);
}

// Configure which pin use to reset the target board
#define PIN_RESET 10

// Whether the device has reset active high or low
static bool device_reset_active_high;

// Reset or resume the target board
void set_target_reset_state(bool reset) {
  if ((reset && device_reset_active_high) ||
      (!reset && !device_reset_active_high)) {
    digitalWrite(PIN_RESET, HIGH);
  } else {
    digitalWrite(PIN_RESET, LOW);
  }
}

// ------------------------------------
// LEDs

// Configure which pins to use for the LEDs:
#define LED_HB 9
#define LED_ERR 8
#define LED_PMODE 7

// This provides a heartbeat_step on LED_HB
uint8_t hb_val = 128;
int8_t hb_delta = 8;

void heartbeat_step() {
  static unsigned long last_time = 0;
  unsigned long now = millis();
  if ((now - last_time) < 40) {
    return;
  }
  last_time = now;
  if (hb_val > 192) {
    hb_delta = -hb_delta;
  }
  if (hb_val < 32) {
    hb_delta = -hb_delta;
  }
  hb_val += hb_delta;
  analogWrite(LED_HB, hb_val);
}

// Pulse an LED a couple of times
#define PULSE_TIME 30

void led_pulse(int pin, int times) {
  do {
    digitalWrite(pin, HIGH);
    delay(PULSE_TIME);
    digitalWrite(pin, LOW);
    delay(PULSE_TIME);
  } while (times--);
}

// Set the state of the different LEDs

void set_led_hb(int state) { digitalWrite(LED_HB, state); }

void set_led_err(int state) { digitalWrite(LED_ERR, state); }

void set_led_pmode(int state) { digitalWrite(LED_PMODE, state); }

// ------------------------------------
// Programming mode (pmode)

int pmode = 0;    // Is the mode enabled?
int ISPError = 0; // Was there any error so far?

unsigned int current_addr; // The current address being read/written from/to

// Send an empty reply
void empty_reply() {
  if (CRC_EOP == get_serial_char()) {
    Serial.print((char)STK_INSYNC);
    Serial.print((char)STK_OK);
  } else {
    ISPError++;
    Serial.print((char)STK_NOSYNC);
  }
}

// Send a one-byte reply
void byte_reply(uint8_t b) {
  if (CRC_EOP == get_serial_char()) {
    Serial.print((char)STK_INSYNC);
    Serial.print((char)b);
    Serial.print((char)STK_OK);
  } else {
    ISPError++;
    Serial.print((char)STK_NOSYNC);
  }
}

// Start programming mode
void start_pmode() {

  // Reset target before driving PIN_SCK or PIN_MOSI
  set_target_reset_state(true);
  pinMode(PIN_RESET, OUTPUT);

  // SPI.begin() will configure SS as output, so SPI master mode is selected.
  // We have defined PIN_RESET as pin 10, which for many Arduinos is not the SS
  // pin. So we have to configure PIN_RESET as output here,
  // (set_target_reset_state() first sets the correct level).
  SPI.begin();
  SPI.beginTransaction(SPISettings(SPI_CLOCK, MSBFIRST, SPI_MODE0));

  // Pulse PIN_RESET after PIN_SCK is low:
  digitalWrite(PIN_SCK, LOW);
  delay(20); // discharge PIN_SCK, value arbitrarily chosen
  set_target_reset_state(false);
  delayMicroseconds(100); // Pulse must be minimum 2 target CPU clock cycles so
                          // 100 usec is ok for CPU speeds above 20 KHz
  set_target_reset_state(true);

  // Send the enable programming command:
  delay(50); // datasheet: must be > 20 msec
  spi_transaction(0xAC, 0x53, 0x00, 0x00);

  pmode = 1;
}

// End programming mode
void end_pmode() {
  SPI.end();

  // We're about to take the target out of reset so configure SPI pins as input
  pinMode(PIN_MOSI, INPUT);
  pinMode(PIN_SCK, INPUT);

  set_target_reset_state(false);
  pinMode(PIN_RESET, INPUT);

  pmode = 0;
}

// ------------------------------------
// Global block buffer

uint8_t buffer[256];

void fill_buffer(int n) {
  for (int x = 0; x < n; x++) {
    buffer[x] = get_serial_char();
  }
}

// ------------------------------------
// Target device parameters

#define BEGET16(addr) (*addr * 256 + *(addr + 1))

typedef struct {
  uint8_t device_code;
  uint8_t revision;
  uint8_t prog_type;
  uint8_t par_mode;
  uint8_t polling;
  uint8_t self_timed;
  uint8_t lock_bytes;
  uint8_t fuse_bytes;
  uint8_t flash_poll;
  uint16_t eeprom_poll;
  uint16_t page_size;
  uint16_t eeprom_size;
  uint32_t flash_size;
} device_parameters;

device_parameters device;

void set_device_parameters() {
  // call this after reading parameter packet into buffer[]
  device.device_code = buffer[0];
  device.revision = buffer[1];
  device.prog_type = buffer[2];
  device.par_mode = buffer[3];
  device.polling = buffer[4];
  device.self_timed = buffer[5];
  device.lock_bytes = buffer[6];
  device.fuse_bytes = buffer[7];
  device.flash_poll = buffer[8];

  // ignore buffer[9] (= buffer[8])
  // following are 16 bits (big endian)
  device.eeprom_poll = BEGET16(&buffer[10]);
  device.page_size = BEGET16(&buffer[12]);
  device.eeprom_size = BEGET16(&buffer[14]);

  // 32 bits flash_size (big endian)
  device.flash_size = buffer[16] * 0x01000000 + buffer[17] * 0x00010000 +
                      buffer[18] * 0x00000100 + buffer[19];

  // AVR devices have active low reset, AT89Sx are active high
  device_reset_active_high = (device.device_code >= 0xe0);
}

// ------------------------------------
// EEPROM read/write

char eeprom_read_page(int length) {
  int start = current_addr * 2;
  for (int x = 0; x < length; x++) {
    int addr = start + x;
    uint8_t ee = spi_transaction(0xA0, (addr >> 8) & 0xFF, addr & 0xFF, 0xFF);
    Serial.print((char)ee);
  }
  return STK_OK;
}

// Write (length) bytes, (start) is a byte address
uint8_t write_eeprom_chunk(unsigned int start, unsigned int length) {
  // this writes byte-by-byte, page writing may be faster (4 bytes at a time)
  fill_buffer(length);
  set_led_pmode(LOW);
  for (unsigned int x = 0; x < length; x++) {
    unsigned int addr = start + x;
    spi_transaction(0xC0, (addr >> 8) & 0xFF, addr & 0xFF, buffer[x]);
    delay(45);
  }
  set_led_pmode(HIGH);
  return STK_OK;
}

#define EEPROM_CHUNK_SIZE 32

uint8_t write_eeprom(unsigned int length) {
  unsigned int start = current_addr * 2;
  unsigned int remaining = length;

  if (length > device.eeprom_size) {
    ISPError++;
    return STK_FAILED;
  }

  while (remaining > EEPROM_CHUNK_SIZE) {
    write_eeprom_chunk(start, EEPROM_CHUNK_SIZE);
    start += EEPROM_CHUNK_SIZE;
    remaining -= EEPROM_CHUNK_SIZE;
  }

  write_eeprom_chunk(start, remaining);

  return STK_OK;
}

// ------------------------------------
// Flash read/write

uint8_t flash_read(uint8_t hilo, unsigned int addr) {
  return spi_transaction(0x20 + hilo * 8, (addr >> 8) & 0xFF, addr & 0xFF, 0);
}

char flash_read_page(int length) {
  for (int x = 0; x < length; x += 2) {

    uint8_t low = flash_read(LOW, current_addr);
    Serial.print((char)low);

    uint8_t high = flash_read(HIGH, current_addr);
    Serial.print((char)high);

    current_addr++;
  }
  return STK_OK;
}

unsigned int current_page() {
  if (device.page_size == 32) {
    return current_addr & 0xFFFFFFF0;
  }
  if (device.page_size == 64) {
    return current_addr & 0xFFFFFFE0;
  }
  if (device.page_size == 128) {
    return current_addr & 0xFFFFFFC0;
  }
  if (device.page_size == 256) {
    return current_addr & 0xFFFFFF80;
  }
  return current_addr;
}

void flash(uint8_t hilo, unsigned int addr, uint8_t data) {
  spi_transaction(0x40 + 8 * hilo, addr >> 8 & 0xFF, addr & 0xFF, data);
}

void commit(unsigned int addr) {
  set_led_pmode(LOW);
  spi_transaction(0x4C, (addr >> 8) & 0xFF, addr & 0xFF, 0);
  delay(PULSE_TIME);
  set_led_pmode(HIGH);
}

uint8_t flash_write_pages(int length) {
  int x = 0;
  unsigned int page = current_page();

  while (x < length) {

    if (page != current_page()) {
      commit(page);
      page = current_page();
    }

    flash(LOW, current_addr, buffer[x++]);
    flash(HIGH, current_addr, buffer[x++]);
    current_addr++;
  }

  commit(page);

  return STK_OK;
}

void flash_write(int length) {
  fill_buffer(length);

  if (CRC_EOP == get_serial_char()) {
    Serial.print((char)STK_INSYNC);
    Serial.print((char)flash_write_pages(length));
  } else {
    ISPError++;
    Serial.print((char)STK_NOSYNC);
  }
}

// ------------------------------------
// Dispatch STK commands

void stk_get_sync() {
  ISPError = 0;
  empty_reply();
}

void stk_get_sign_on() {
  if (get_serial_char() == CRC_EOP) {
    Serial.print((char)STK_INSYNC);
    Serial.print("AVR ISP");
    Serial.print((char)STK_OK);
  } else {
    ISPError++;
    Serial.print((char)STK_NOSYNC);
  }
}

void stk_get_parameter() {
  uint8_t ch = get_serial_char();

  switch (ch) {
  case 0x80: // STK_HW_VER
    byte_reply(HW_VER);
    break;
  case 0x81: // STK_SW_MAJOR
    byte_reply(SW_MAJOR);
    break;
  case 0x82: // ST_SW_MINOR
    byte_reply(SW_MINOR);
    break;
  case 0x93:
    byte_reply('S'); // Serial programmer
    break;
  default:
    byte_reply(0);
  }
}

void stk_set_device() {
  fill_buffer(20);
  set_device_parameters();
  empty_reply();
}

void stk_set_device_ext() {
  fill_buffer(5);
  empty_reply();
}

void stk_enter_progmode() {
  if (!pmode) {
    start_pmode();
  }
  empty_reply();
}

void stk_leave_progmode() {
  end_pmode();
  empty_reply();
}

void stk_load_address() {
  current_addr = get_serial_char();
  current_addr += 256 * get_serial_char();
  empty_reply();
}

void stk_universal() {
  fill_buffer(4);
  uint8_t ch = spi_transaction(buffer[0], buffer[1], buffer[2], buffer[3]);
  byte_reply(ch);
}

void stk_prog_flash() {
  get_serial_char(); // low addr
  get_serial_char(); // high addr
  empty_reply();
}

void stk_prog_data() {
  get_serial_char(); // data
  empty_reply();
}

void stk_prog_page() {
  char result = (char)STK_FAILED;
  unsigned int length = 256 * get_serial_char();
  length += get_serial_char();
  char memtype = get_serial_char();

  if (memtype == 'F') {
    flash_write(length);
    return;
  }

  if (memtype == 'E') {
    result = (char)write_eeprom(length);
    if (CRC_EOP == get_serial_char()) {
      Serial.print((char)STK_INSYNC);
      Serial.print(result);
    } else {
      ISPError++;
      Serial.print((char)STK_NOSYNC);
    }
    return;
  }

  Serial.print((char)STK_FAILED);
  return;
}

void stk_read_page() {
  char result = (char)STK_FAILED;

  int length = 256 * get_serial_char();
  length += get_serial_char();
  char memtype = get_serial_char();

  if (CRC_EOP != get_serial_char()) {
    ISPError++;
    Serial.print((char)STK_NOSYNC);
    return;
  }

  Serial.print((char)STK_INSYNC);

  if (memtype == 'F') {
    result = flash_read_page(length);
  }

  if (memtype == 'E') {
    result = eeprom_read_page(length);
  }

  Serial.print(result);
}

void stk_read_sign() {

  if (CRC_EOP != get_serial_char()) {
    ISPError++;
    Serial.print((char)STK_NOSYNC);
    return;
  }

  Serial.print((char)STK_INSYNC);

  uint8_t high = spi_transaction(0x30, 0x00, 0x00, 0x00);
  Serial.print((char)high);

  uint8_t middle = spi_transaction(0x30, 0x00, 0x01, 0x00);
  Serial.print((char)middle);

  uint8_t low = spi_transaction(0x30, 0x00, 0x02, 0x00);
  Serial.print((char)low);

  Serial.print((char)STK_OK);
}

// The command dispatcher
void dispatch_stk_command(uint8_t cmd) {
  switch (cmd) {

  case STK_GET_SYNC:
    stk_get_sync();
    break;

  case STK_GET_SIGN_ON:
    stk_get_sign_on();
    break;

  case STK_GET_PARAMETER:
    stk_get_parameter();
    break;

  case STK_SET_DEVICE:
    stk_set_device();
    break;

  case SET_DEVICE_EXT:
    stk_set_device_ext();
    break;

  case STK_ENTER_PROGMODE:
    stk_enter_progmode();
    break;

  case STK_LEAVE_PROGMODE:
    stk_leave_progmode();
    break;

  case STK_LOAD_ADDRESS:
    stk_load_address();
    break;

  case STK_UNIVERSAL:
    stk_universal();
    break;

  case STK_PROG_FLASH:
    stk_prog_flash();
    break;

  case STK_PROG_DATA:
    stk_prog_data();
    break;

  case STK_PROG_PAGE:
    stk_prog_page();
    break;

  case STK_READ_PAGE:
    stk_read_page();
    break;

  case STK_READ_SIGN:
    stk_read_sign();
    break;

  // Expecting a command, not CRC_EOP
  // This is how we can get back in sync
  case CRC_EOP:
    ISPError++;
    Serial.print((char)STK_NOSYNC);
    break;

  // Anything else we will return STK_UNKNOWN
  default:
    ISPError++;
    if (CRC_EOP == get_serial_char()) {
      Serial.print((char)STK_UNKNOWN);
    } else {
      Serial.print((char)STK_NOSYNC);
    }
  }
}

// ------------------------------------
// setup() and loop()

void setup() {
  Serial.begin(BAUDRATE);

  pinMode(LED_HB, OUTPUT);
  led_pulse(LED_HB, 2);

  pinMode(LED_ERR, OUTPUT);
  led_pulse(LED_ERR, 2);

  pinMode(LED_PMODE, OUTPUT);
  led_pulse(LED_PMODE, 2);
}

void loop() {

  // React to input messages
  if (Serial.available()) {
    uint8_t cmd = get_serial_char();
    dispatch_stk_command(cmd);
  }

  // Update the heartbeat LED
  heartbeat_step();

  // Is there an error?
  set_led_err(ISPError);

  // Is pmode active?
  set_led_pmode(pmode);

}