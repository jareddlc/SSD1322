#ifndef SSD1322_h
#define SSD1322_h

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class SSD1322 {
public:
  SSD1322(int cs, int dc, int reset, int sclk, int sdin, int spi_host);
  void send_ssd1322_command(unsigned char d);
  void send_ssd1322_data(unsigned char d);
  void send_ssd1322_data_buffer(const uint8_t* data, size_t length);
  void send_spi_transaction(int mode, const uint8_t* data, size_t length);
  void init(int columns, int rows);
  void test();

  // SSD1322 specific functions
  void reset_device();
  void fill_ram(unsigned char d);

  // 10.1.1 Enable Gray Scale Table (00h)
  // 10.1.2 Set Column Address (15h)
  void set_column_address(unsigned char d, unsigned char e);
  // 10.1.3 Write RAM Command (5Ch)
  void set_write_ram();
  // 10.1.4 Read RAM Command (5Dh)
  // 10.1.5 Set Row Address (75h)
  void set_row_address(unsigned char d, unsigned char e);
  // 10.1.6 Set Re-map & Dual COM Line Mode (A0h)
  void set_remap_dual_com_line_mode(unsigned char d);
  // 10.1.7 Set Display Start Line (A1h)
  void set_display_start_line(unsigned char d);
  // 10.1.8 Set Display Offset (A2h)
  void set_display_offset(unsigned char d);
  // 10.1.9 Set Display Mode (A4h ~ A7h)
  void set_display_mode(unsigned char d);
  // 10.1.10 Enable Partial Display (A8h)
  // 10.1.11 Exit Partial Display (A9h)
  void set_exit_partial_display();
  // 10.1.12 Set Function selection (ABh)
  void set_function_selection(unsigned char d);
  // 10.1.13 Set Display ON/OFF (AEh / AFh)
  void set_display_on_off(unsigned char d);
  // 10.1.14 Set Phase Length (B1h)
  void set_phase_length(unsigned char d);
  // 10.1.15 Set Front Clock Divider / Oscillator Frequency (B3h)
  void set_front_clock_divider(unsigned char d);
  // 10.1.16 Display Enhancement A (B4h)
  void set_display_enhancement_a(unsigned char d, unsigned char e);
  // 10.1.17 Set GPIO (B5h)
  // 10.1.18 Set Second Pre-charge period (B6h)
  // 10.1.19 Set Gray Scale Table (B8h)
  // 10.1.20 Select Default Linear Gray Scale Table (B9h)
  void set_default_linear_gray_scale_table();
  // 10.1.21 Set Pre-charge voltage (BBh)
  void set_precharge_voltage(unsigned char d);
  // 10.1.22 Set VCOMH Voltage (BEh)
  void set_vcomh_voltage(unsigned char d);
  // 10.1.23 Set Contrast Current (C1h)
  void set_contrast_current(unsigned char d);
  // 10.1.24 Master Current Control (C7h)
  void set_master_current_control(unsigned char d);
  // 10.1.25 Set Multiplex Ratio (CAh)
  void set_multiplex_ratio(unsigned char d);
  // 10.1.26 Display Enhancement B (D1h)
  // 10.1.27 Set Command Lock (FDh)
  void set_command_lock(unsigned char d);

  // SSD1322 commands
  unsigned char ENABLE_GRAY_SCALE_TABLE                = 0x00;
  unsigned char SET_COLUMN_ADDRESS                     = 0x15;
  unsigned char WRITE_RAM                              = 0x5C;
  unsigned char READ_RAM                               = 0x5D;
  unsigned char SET_ROW_ADDRESS                        = 0x75;
  unsigned char SET_REMAP_DUAL_COM_LINE_MODE           = 0xA0;
  unsigned char SET_DISPLAY_START_LINE                 = 0xA1;
  unsigned char SET_DISPLAY_OFFSET                     = 0xA2;
  unsigned char SET_DISPLAY_MODE_MASK                  = 0xA4;
  unsigned char PARTIAL_DISPLAY_MASK                   = 0xA8;
  unsigned char EXIT_PARTIAL_DISPLAY                   = 0xA9;
  unsigned char SET_FUNCTION_SELECTION                 = 0xAB;
  unsigned char DISPLAY_ON_OFF_MASK                    = 0xAE;
  unsigned char SET_PHASE_LENGTH                       = 0xB1;
  unsigned char SET_FRONT_CLOCK_DIVIDER                = 0xB3;
  unsigned char DISPLAY_ENHANCEMENT_A                  = 0xB4;
  unsigned char SET_GPIO                               = 0xB5;
  unsigned char SET_SECOND_PRECHARGE_PERIOD            = 0xB6;
  unsigned char SET_GRAY_SCALE_TABLE                   = 0xB8;
  unsigned char SELECT_DEFAULT_LINEAR_GRAY_SCALE_TABLE = 0xB9;
  unsigned char SET_PRECHARGE_VOLTAGE                  = 0xBB;
  unsigned char SET_VCOMH_VOLTAGE                      = 0xBE;
  unsigned char SET_CONTRAST_CURRENT                   = 0xC1;
  unsigned char MASTER_CURRENT_CONTROL                 = 0xC7;
  unsigned char SET_MULTIPLEX_RATIO                    = 0xCA;
  unsigned char DISPLAY_ENHANCEMENT_B                  = 0xD1;
  unsigned char SET_COMMAND_LOCK                       = 0xFD;
  // GDDRAM dimensions in bytes
  // Actual width is 480 pixels but each pixel is 4 bits wide
  // GDDRAM - Graphics Display Data RAM
  unsigned int GDDRAM_WIDTH                           = 240U;
  unsigned int GDDRAM_HEIGHT                          = 128U;
  // Dimensions of physical display in pixels
  unsigned int DISPLAY_WIDTH                          = 256U;
  unsigned int DISPLAY_HEIGHT                         = 64U;
  // Options for turning display on or off
  unsigned char DISPLAY_ON                             = 0xAF; // 0x01;
  unsigned char DISPLAY_OFF                            = 0xAE; // 0x00
  // Options for controlling partial display
  unsigned char ENABLE_PARTIAL_DISPLAY                 = 0x00;
  unsigned char DISABLE_PARTIAL_DISPLAY                = 0x01;
  // Options for display mode
  unsigned char DISPLAY_MODE_DEFAULT                   = 0x00;
  unsigned char DISPLAY_MODE_OFF                       = 0x00;
  unsigned char DISPLAY_MODE_ON                        = 0x01;
  unsigned char DISPLAY_MODE_NORMAL                    = 0x02;
  unsigned char DISPLAY_MODE_INVERSE                   = 0x03;
  // Options for controlling VSL selection
  unsigned char ENABLE_EXTERNAL_VSL                    = 0x00;
  unsigned char ENABLE_INTERNAL_VSL                    = 0x02;
  // Options for grayscale quality
  unsigned char NORMAL_GRAYSCALE_QUALITY               = 0xB0;
  unsigned char ENHANCED_LOW_GRAY_SCALE_QUALITY        = 0XF8;
  // Options for display enhancement b
  unsigned char NORMAL_ENHANCEMENT                     = 0x20;
  // Options for command lock
  unsigned char COMMANDS_LOCK                          = 0x16;
  unsigned char COMMANDS_UNLOCK                        = 0x12;

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
};

#endif