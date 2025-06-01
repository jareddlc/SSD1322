#ifndef SSD1322_h
#define SSD1322_h

#include <driver/gpio.h>
#include <driver/spi_master.h>

#include <lvgl.h>

class SSD1322 {
public:
  SSD1322(int cs, int dc, int reset, int sclk, int sdin, int spi_host);
  void init(int columns, int rows, bool is_async);
  void send_command(uint8_t d);
  void send_data(uint8_t d);
  void send_buffer(const uint8_t *data, size_t length);
  void send_buffer_async(const uint8_t *data, size_t length, void *disp);
  void send_buffer_chunked(uint8_t *data, size_t length, size_t chunk_size);
  void send_buffer_chunked_async(uint8_t *data, size_t length, size_t chunk_size, void *disp);
  void send_spi_transaction(uint8_t mode, const uint8_t *data, size_t length);
  void send_spi_transaction_async(uint8_t mode, const uint8_t *data, size_t length, void *disp);
  void test();

  // SSD1322 specific functions
  void reset_device();
  void fill_ram(uint8_t d);
  void fill_ram_480_128(uint8_t d);

  // LVGL specific functions
  // void lvgl_set_pixel_buffer(uint8_t *buffer);
  // static void IRAM_ATTR lvgl_align_area(lv_event_t *e);
  // void IRAM_ATTR lvgl_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map);

  // 10.1.1 Enable Gray Scale Table (00h)
  // 10.1.2 Set Column Address (15h)
  void set_column_address(uint8_t d, uint8_t e);
  // 10.1.3 Write RAM Command (5Ch)
  void set_write_ram();
  // 10.1.4 Read RAM Command (5Dh)
  // 10.1.5 Set Row Address (75h)
  void set_row_address(uint8_t d, uint8_t e);
  // 10.1.6 Set Re-map & Dual COM Line Mode (A0h)
  void set_remap_dual_com_line_mode(uint8_t d);
  // 10.1.7 Set Display Start Line (A1h)
  void set_display_start_line(uint8_t d);
  // 10.1.8 Set Display Offset (A2h)
  void set_display_offset(uint8_t d);
  // 10.1.9 Set Display Mode (A4h ~ A7h)
  void set_display_mode(uint8_t d);
  // 10.1.10 Enable Partial Display (A8h)
  // 10.1.11 Exit Partial Display (A9h)
  void set_exit_partial_display();
  // 10.1.12 Set Function selection (ABh)
  void set_function_selection(uint8_t d);
  // 10.1.13 Set Display ON/OFF (AEh / AFh)
  void set_display_on_off(uint8_t d);
  // 10.1.14 Set Phase Length (B1h)
  void set_phase_length(uint8_t d);
  // 10.1.15 Set Front Clock Divider / Oscillator Frequency (B3h)
  void set_front_clock_divider(uint8_t d);
  // 10.1.16 Display Enhancement A (B4h)
  void set_display_enhancement_a(uint8_t d, uint8_t e);
  // 10.1.17 Set GPIO (B5h)
  // 10.1.18 Set Second Pre-charge period (B6h)
  void set_second_precharge_period(uint8_t d);
  // 10.1.19 Set Gray Scale Table (B8h)
  void set_gray_scale_table(const uint8_t* d, size_t l);
  // 10.1.20 Select Default Linear Gray Scale Table (B9h)
  void set_default_linear_gray_scale_table();
  // 10.1.21 Set Pre-charge voltage (BBh)
  void set_precharge_voltage(uint8_t d);
  // 10.1.22 Set VCOMH Voltage (BEh)
  void set_vcomh_voltage(uint8_t d);
  // 10.1.23 Set Contrast Current (C1h)
  void set_contrast_current(uint8_t d);
  // 10.1.24 Master Current Control (C7h)
  void set_master_current_control(uint8_t d);
  // 10.1.25 Set Multiplex Ratio (CAh)
  void set_multiplex_ratio(uint8_t d);
  // 10.1.26 Display Enhancement B (D1h)
  void set_display_enhancement_b(uint8_t d, uint8_t e);
  // 10.1.27 Set Command Lock (FDh)
  void set_command_lock(uint8_t d);

  enum COMMAND : uint8_t {
    ENABLE_GRAY_SCALE_TABLE = 0x00,
    SET_COLUMN_ADDRESS = 0x15,
    WRITE_RAM = 0x5C,
    READ_RAM = 0x5D,
    SET_ROW_ADDRESS = 0x75,
    SET_REMAP_DUAL_COM_LINE_MODE = 0xA0,
    SET_DISPLAY_START_LINE = 0xA1,
    SET_DISPLAY_OFFSET = 0xA2,
    SET_DISPLAY_MODE_MASK = 0xA4,
    PARTIAL_DISPLAY_MASK = 0xA8,
    EXIT_PARTIAL_DISPLAY = 0xA9,
    SET_FUNCTION_SELECTION = 0xAB,
    DISPLAY_ON_OFF_MASK = 0xAE,
    SET_PHASE_LENGTH = 0xB1,
    SET_FRONT_CLOCK_DIVIDER = 0xB3,
    DISPLAY_ENHANCEMENT_A = 0xB4,
    SET_GPIO = 0xB5,
    SET_SECOND_PRECHARGE_PERIOD = 0xB6,
    SET_GRAY_SCALE_TABLE = 0xB8,
    SELECT_DEFAULT_LINEAR_GRAY_SCALE_TABLE = 0xB9,
    SET_PRECHARGE_VOLTAGE = 0xBB,
    SET_VCOMH_VOLTAGE = 0xBE,
    SET_CONTRAST_CURRENT = 0xC1,
    MASTER_CURRENT_CONTROL = 0xC7,
    SET_MULTIPLEX_RATIO = 0xCA,
    DISPLAY_ENHANCEMENT_B = 0xD1,
    SET_COMMAND_LOCK = 0xFD,
    // Display On/Off Options
    DISPLAY_ON = 0xAF,
    DISPLAY_OFF = 0xAE,
    // Display Mode Commands
    DISPLAY_MODE_OFF = 0xA4,
    DISPLAY_MODE_ON = 0xA5,
    DISPLAY_MODE_NORMAL = 0xA6,
    DISPLAY_MODE_INVERSE = 0xA7,
    // Options for command lock
    COMMANDS_LOCK = 0x16,
    COMMANDS_UNLOCK = 0x12,
  };

private:
  gpio_num_t cs;
  gpio_num_t dc;
  gpio_num_t reset;
  gpio_num_t sclk;
  gpio_num_t sdin;
  int spi_host;
  spi_device_handle_t spi;
  int columns;
  int rows;
  bool is_async;
  // uint8_t *pixel_buff;
};

#endif