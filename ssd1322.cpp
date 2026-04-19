#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "ssd1322.h"

// cs - SPI chip select
// dc - SPI data/command
// reset - Reset
// sclk - SPI Clock
// sdin - SPI MOSI
// spi_host - ESP32 SPI host (SPI_HOST = 0 [SPI1], HSPI_HOST = 1 [SPI2], VSPI_HOST = 2 [SPI3])
SSD1322::SSD1322(int cs, int dc, int reset, int sclk, int sdin, int spi_host) {
  this->cs = (gpio_num_t)cs;
  this->dc = (gpio_num_t)dc;
  this->reset = (gpio_num_t)reset;
  this->sclk = (gpio_num_t)sclk;
  this->sdin = (gpio_num_t)sdin;
  this->spi_host = spi_host;
}

void SSD1322::init(int columns, int rows, spi_callback_t post_cb) {
  // set oled size
  this->columns = columns;
  this->rows = rows;
  if (columns == 256) {
    this->col_offset = OFFSET_256_64;
  } else {
    this->col_offset = OFFSET_NONE;
  }

  // async queue
  this->queue_size = (post_cb != nullptr) ? 10 : 1;
  this->curr_trans_idx = 0;

  // allocate the trans config pool
  this->trans_config_pool = (ssd1322_trans_config_t*)heap_caps_malloc(sizeof(ssd1322_trans_config_t) * queue_size, MALLOC_CAP_DMA);

  // allocate transaction structures in DMA capable RAM
  this->trans_pool = (spi_transaction_t*)heap_caps_malloc(sizeof(spi_transaction_t) * queue_size, MALLOC_CAP_DMA);
  memset(this->trans_pool, 0, sizeof(spi_transaction_t) * queue_size);

  // initialize GPIO
  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = ((1ULL << this->dc) | (1ULL << this->reset) | (1ULL << this->cs));
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&io_conf);

  // create SPI bus
  spi_bus_config_t buscfg = {};
  buscfg.sclk_io_num = this->sclk;
  buscfg.mosi_io_num = this->sdin;
  buscfg.miso_io_num = GPIO_NUM_NC;
  buscfg.quadwp_io_num = GPIO_NUM_NC;
  buscfg.quadhd_io_num = GPIO_NUM_NC;
  buscfg.max_transfer_sz = 8192;

  // create SPI device
  spi_device_interface_config_t devcfg = {};
  devcfg.spics_io_num = this->cs;
  devcfg.clock_speed_hz = SPI_MASTER_FREQ_20M; //SPI_MASTER_FREQ_8M
  devcfg.mode = 0;
  devcfg.clock_source = SPI_CLK_SRC_DEFAULT; // SOC_MOD_CLK_APB;
  devcfg.address_bits = 0;
  devcfg.command_bits = 0;
  devcfg.dummy_bits = 0;
  devcfg.duty_cycle_pos = 0;
  devcfg.cs_ena_posttrans = 0;
  devcfg.cs_ena_pretrans = 0;
  devcfg.flags = 0;
  devcfg.pre_cb = [](spi_transaction_t *t) {
    ssd1322_trans_config_t *conf = (ssd1322_trans_config_t*)t->user;
    if (conf) {
      gpio_set_level(conf->dc_pin, conf->mode);
    }
  };
  devcfg.post_cb = post_cb;
  devcfg.queue_size = this->queue_size;

  // init SPI
  spi_bus_initialize((spi_host_device_t)this->spi_host, &buscfg, SPI_DMA_CH_AUTO);
  spi_bus_add_device((spi_host_device_t)this->spi_host, &devcfg, &this->spi);

  this->reset_device();

  // initialization sequence
  this->init_sequence();
  // let voltages stabilize
  vTaskDelay(pdMS_TO_TICKS(100));
}

void SSD1322::init_sequence() {
  // SSD1322 optimized for high contrast & speed
  this->set_command_lock(this->COMMAND::COMMANDS_UNLOCK);
  this->set_display_on_off(this->COMMAND::DISPLAY_OFF);

  // timing & driving scheme
  this->set_front_clock_divider(0xF1);               // High refresh rate for smoother LVGL animations. Other: 0xF2
  this->set_multiplex_ratio(0x3F);
  this->set_display_offset(0x00);
  this->set_display_start_line(0x00);
  this->set_remap_dual_com_line_mode(0x14, 0x11);    // Standard mapping
  this->set_function_selection(0x01);                // Internal VDD regulator

  // hardware stability fixes (crucial for 2nd Gen SSD1322)
  this->set_display_enhancement_a(0xA0, 0xFD);
  this->set_display_enhancement_b(0x82, 0x20);       // Fixes pixel discharge "ghosting"

  // brightness & grayscale Logic
  uint8_t gray_scale_table[15] = {40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180};
  this->set_gray_scale_table(gray_scale_table, 15);
  this->set_contrast_current(0x9F);                  // High contrast without overdriving (prolongs OLED life). Max: 0xFF
  this->set_master_current_control(0x0F);            // Max: 0x0F

  // voltage and phase (fine-tuned for sharpe edges)
  this->set_phase_length(0x32);                      // Sharpens pixel transitions
  this->set_second_precharge_period(0); // 0x08
  this->set_precharge_voltage(0x1F);                // 0.6*VCC
  this->set_vcomh_voltage(0x07);                    // 0.86*VCC - Keeps blacks deep

  // finalize
  this->set_display_mode(this->COMMAND::DISPLAY_MODE_NORMAL);
  this->set_exit_partial_display();
  this->fill_ram_256_64(0x00);
  this->set_display_on_off(this->COMMAND::DISPLAY_ON);
}

void SSD1322::init_sequence_datasheet() {
  // SSD1322 datasheet standard init
  this->set_command_lock(0x12);                      // Unlock (Command 0xFD)
  this->set_display_on_off(0xAE);                    // Display OFF (Command 0xAE)
  this->set_front_clock_divider(0x91);               // 105Hz frame rate (Command 0xB3)
  this->set_multiplex_ratio(0x3F);                   // 1/64 Duty (Command 0xCA)
  this->set_display_offset(0x00);                    // No offset (Command 0xD3)
  this->set_display_start_line(0x00);                // Start line 0 (Command 0xA1)
  this->set_remap_dual_com_line_mode(0x14, 0x11);    // Horizontal address increment, Disable Dual COM (Command 0xA0)
  this->set_function_selection(0x01);                // Enable internal VDD (Command 0xAB)
  this->set_display_enhancement_a(0xA0, 0xFD);       // Default enhancement (Command 0xB4)
  this->set_contrast_current(0x7F);                  // Mid brightness (Command 0xC1) (other: 0x9F)
  this->set_master_current_control(0x0F);            // Max range (Command 0xC7)
  this->set_default_linear_gray_scale_table();       // Standard Gamma (Command 0xB9)
  this->set_phase_length(0xE2);                      // Phase 1: 5 DCLKs, Phase 2: 14 DCLKs (Command 0xB1)
  this->set_precharge_voltage(0x1F);                 // 0.60 x VCC (Command 0xBB)
  this->set_display_mode(0xA6);                      // Normal display mode (Command 0xA6)
  this->set_vcomh_voltage(0x07);                     // 0.86 x VCC (Command 0xBE)
  this->set_second_precharge_period(0x08);           // 8 DCLKs (Command 0xB6)
  // this->set_exit_partial_display();
  this->fill_ram_256_64(0x00);                       // Clear screen
  this->set_display_on_off(0xAF);                    // Display ON (Command 0xAF)
}


void SSD1322::send_spi_transaction(uint8_t mode, const uint8_t *data, size_t length) {
  spi_transaction_t spi_transaction = {};
  spi_transaction.length = length * 8;
  spi_transaction.tx_buffer = data;

  gpio_set_level(this->dc, mode);
  spi_device_polling_transmit(this->spi, &spi_transaction);
}

void IRAM_ATTR SSD1322::send_spi_transaction_async(uint8_t mode, const uint8_t *data, size_t length, void *display) {
  // get the next available transaction/config slot from pool
  spi_transaction_t *transaction = &this->trans_pool[this->curr_trans_idx];
  ssd1322_trans_config_t *config = &this->trans_config_pool[this->curr_trans_idx];

  // setup the config
  config->dc_pin = this->dc;
  config->mode = mode;
  config->lv_display = display;

  // setup the transaction
  transaction->length = length * 8;
  transaction->rxlength = 0;
  transaction->tx_buffer = data;
  // store the config
  transaction->user = (void *)config;

  // queue transaction. pre_cb will set the DC pin
  spi_device_queue_trans(this->spi, transaction, portMAX_DELAY);

  // increment index
  this->curr_trans_idx = (this->curr_trans_idx + 1) % this->queue_size;
}

void SSD1322::send_command(uint8_t d) {
  this->send_spi_transaction(0, &d, 1);
}

void SSD1322::send_data(uint8_t d) {
  this->send_spi_transaction(1, &d, 1);
}

void SSD1322::send_buffer(const uint8_t *data, size_t length) {
  this->send_spi_transaction(1, data, length);
}

void SSD1322::send_buffer_async(const uint8_t *data, size_t length, void *disp) {
  this->send_spi_transaction_async(1, data, length, disp);
}

void SSD1322::send_buffer_chunked(uint8_t *data, size_t length, size_t chunk_size) {
  for (size_t offset = 0; offset < length; offset += chunk_size) {
    size_t len = (offset + chunk_size <= length) ? chunk_size : (length - offset);
    this->send_buffer(data + offset, len);
  }
}

void SSD1322::send_buffer_chunked_async(uint8_t *data, size_t length, size_t chunk_size, void *disp) {
  for (size_t offset = 0; offset < length; offset += chunk_size) {
    size_t len = (offset + chunk_size <= length) ? chunk_size : (length - offset);
    this->send_buffer_async(data + offset, len, disp);
  }
}

void SSD1322::reset_device() {
  // to reset set RESET pin to logic LOW for at least 100us
  // and then logic HIGH (fom SSD1322 manual)
  gpio_set_level(this->reset, 0);
  vTaskDelay(pdMS_TO_TICKS(300));
  gpio_set_level(this->reset, 1);
  vTaskDelay(pdMS_TO_TICKS(300));
}

// SSD1322 Memory Mapping Table:
// +------------+--------+---------+----------------------------+-----------------------+
// | Resolution | Pixels | Bytes   | Column Address Range       | Row Address Range     |
// |            |        | Per Row | (Internal Cmd 15h)         | (Internal Cmd 75h)    |
// +------------+--------+---------+----------------------------+-----------------------+
// | 480 × 128  | 480 px | 240 B   | 0x00 - 0x77 (Dec: 0 - 119) | 0x00 - 0x7F (0 - 127) |
// +------------+--------+---------+----------------------------+-----------------------+
// note: total max RAM for SSD1322 is 120 column addresses (480px) x 128 rows.
// column addressing: 1 address = 4 pixels (2 bytes)
// row addressing: 1 address = 1 pixel
void SSD1322::fill_ram_480_128(uint8_t d) {
  this->fill_ram(480, 128, 0);
}

// SSD1322 Centering Table (256x64 Display on 480x128 Controller)
// +-------------------+--------+---------+--------------------+----------------------+
// | Feature           | Pixels | Bytes   | Addresses (Col)    | Range (Hex)          |
// +-------------------+--------+---------+--------------------+----------------------+
// | Full Controller   | 480 px | 240 B   | 120                | 0x00 to 0x77         |
// | Left Guard Band   | 112 px | 56 B    | 28                 | 0x00 to 0x1B         |
// | Visible Screen    | 256 px | 128 B   | 64                 | 0x1C to 0x5B         |
// | Right Guard Band  | 112 px | 56 B    | 28                 | 0x5C to 0x77         |
// +-------------------+--------+---------+--------------------+----------------------+
// | Total             | 480 px | 240 B   | 120                | 0x00 to 0x77         |
// +-------------------+--------+---------+--------------------+----------------------+
// note: offset = 112px / 4 pixels per address = 28 (0x1C)
void SSD1322::fill_ram_256_64(uint8_t d) {
  this->fill_ram(256, 64, 0);
}


void SSD1322::fill_ram(int cols, int rows, uint8_t d) {
  // calculate the horizontal offset to center the requested width
  // example: (480 - 256) / 2 = 112 pixels
  int pixel_offset = (480 - cols) / 2;

  // convert pixels to addressable window
  // 1 address = 4 pixels
  uint8_t start_addr = (uint8_t)(pixel_offset / 4);
  uint8_t end_addr = (uint8_t)(start_addr + (cols / 4) - 1);

  // set the address window
  this->set_column_address(start_addr, end_addr);
  this->set_row_address(0, rows - 1);
  this->set_write_ram();

  // create a line buffer for spi transaction
  // 2 pixels per byte
  size_t bytes_per_row = cols / 2;
  uint8_t* row_buf = (uint8_t*)malloc(bytes_per_row);

  if (row_buf) {
    memset(row_buf, d, bytes_per_row);

    for (int r = 0; r < rows; r++) {
      // send the whole row at once
      this->send_buffer(row_buf, bytes_per_row);
    }

    free(row_buf);
  }
}

void SSD1322::set_column_address(uint8_t d, uint8_t e) {
  this->send_command(this->COMMAND::SET_COLUMN_ADDRESS);
  this->send_data(d); // default => 0x00
  this->send_data(e); // default => 0x77
}

void SSD1322::set_write_ram() {
  this->send_command(this->COMMAND::WRITE_RAM);
}

void SSD1322::set_row_address(uint8_t d, uint8_t e) {
  this->send_command(this->COMMAND::SET_ROW_ADDRESS);
  this->send_data(d); // default => 0x00
  this->send_data(e); // default => 0x7F
}

void SSD1322::set_remap_dual_com_line_mode(uint8_t d, uint8_t e) {
  this->send_command(this->COMMAND::SET_REMAP_DUAL_COM_LINE_MODE);
  this->send_data(d); // 0x14 NORMAL, 0x06 FLIP?, 0x16?
  this->send_data(e); // default => 0x01 (Disable Dual COM Mode)
}

void SSD1322::set_display_start_line(uint8_t d) {
  this->send_command(this->COMMAND::SET_DISPLAY_START_LINE);
  this->send_data(d);
}

void SSD1322::set_display_offset(uint8_t d) {
  this->send_command(this->COMMAND::SET_DISPLAY_OFFSET);
  this->send_data(d);
}

void SSD1322::set_display_mode(uint8_t d) {
  this->send_command(d);
}

void SSD1322::set_exit_partial_display() {
  this->send_command(this->COMMAND::EXIT_PARTIAL_DISPLAY);
}

void SSD1322::set_function_selection(uint8_t d) {
  this->send_command(this->COMMAND::SET_FUNCTION_SELECTION);
  this->send_data(d);
}

void SSD1322::set_display_on_off(uint8_t d) {
  this->send_command(d);
}

void SSD1322::set_phase_length(uint8_t d) {
  this->send_command(this->COMMAND::SET_PHASE_LENGTH);
  this->send_data(d);
}

void SSD1322::set_front_clock_divider(uint8_t d) {
  this->send_command(this->COMMAND::SET_FRONT_CLOCK_DIVIDER);
  this->send_data(d);
}

void SSD1322::set_display_enhancement_a(uint8_t d, uint8_t e) {
  this->send_command(this->COMMAND::DISPLAY_ENHANCEMENT_A);
  this->send_data(d);
  this->send_data(e);
}

void SSD1322::set_second_precharge_period(uint8_t d) {
  this->send_command(this->COMMAND::SET_SECOND_PRECHARGE_PERIOD);
  this->send_data(d);
}

void SSD1322::set_gray_scale_table(const uint8_t* d, size_t l) {
  this->send_command(this->COMMAND::SET_GRAY_SCALE_TABLE);
  this->send_buffer(d, l);
  this->send_command(this->COMMAND::ENABLE_GRAY_SCALE_TABLE);
}

void SSD1322::set_default_linear_gray_scale_table() {
  this->send_command(this->COMMAND::SELECT_DEFAULT_LINEAR_GRAY_SCALE_TABLE);
}

void SSD1322::set_precharge_voltage(uint8_t d) {
  this->send_command(this->COMMAND::SET_PRECHARGE_VOLTAGE);
  this->send_data(d);
}

void SSD1322::set_vcomh_voltage(uint8_t d) {
  this->send_command(this->COMMAND::SET_VCOMH_VOLTAGE);
  this->send_data(d);
}

void SSD1322::set_contrast_current(uint8_t d) {
  this->send_command(this->COMMAND::SET_CONTRAST_CURRENT);
  this->send_data(d);
}

void SSD1322::set_master_current_control(uint8_t d) {
  this->send_command(this->COMMAND::MASTER_CURRENT_CONTROL);
  this->send_data(d);
}

void SSD1322::set_multiplex_ratio(uint8_t d) {
  this->send_command(this->COMMAND::SET_MULTIPLEX_RATIO);
  this->send_data(d);
}

void SSD1322::set_display_enhancement_b(uint8_t d, uint8_t e) {
  this->send_command(this->COMMAND::DISPLAY_ENHANCEMENT_B);
  this->send_data(d);
  this->send_data(e);
}

void SSD1322::set_command_lock(uint8_t d) {
  this->send_command(this->COMMAND::SET_COMMAND_LOCK);
  this->send_data(d);
}

void SSD1322::test() {
  // COORDINATE SETUP
  // These represent the logical area we want to fill on our specific panel (256x64).
  uint8_t col_start = 0;
  uint8_t row_start = 0;
  uint8_t col_end = this->columns - 1;  // 256
  uint8_t row_end = this->rows - 1;     // 64

  // MEMORY CALCULATION
  // SSD1322 uses 4 bits per pixel (16 grayscale levels).
  // This means 2 pixels fit into 1 byte.
  // Buffer Size = (Total Pixels) / 2
  size_t buffer_size = (this->columns * this->rows) / 2;
  uint8_t* buffer = (uint8_t*)malloc(buffer_size);
  if (!buffer) return;

  // HARDWARE WINDOW ADDRESSING
  // SSD1322 internal RAM is 480 pixels wide (120 addresses).
  // Each 'Address' increment moves the pointer by 4 horizontal pixels.
  // We use 'col_offset' to center our 256px screen in the 480px RAM space.
  uint8_t start_addr = this->col_offset + (col_start / 4);
  uint8_t end_addr = this->col_offset + (col_end / 4);

  this->set_column_address(start_addr, end_addr);
  this->set_row_address(row_start, row_end);
  this->set_write_ram();

  // PATTERN GENERATION
  const int tile_size = 8; // Size of each checker square in pixels
  const uint8_t grayscales[] = { 0x0, 0x5, 0xA, 0xF }; // Black, Dark Gray, Light Gray, White
  const int num_levels = sizeof(grayscales) / sizeof(grayscales[0]);

  int buffer_idx = 0;
  for (int y = row_start; y <= row_end; y++) {
    // We increment 'x' by 2 because we pack two 4-bit pixels into every 1-byte write
    for (int x = col_start; x <= col_end; x += 2) {

      // Calculate which "tile" we are currently in
      int tile_x = x / tile_size;
      int tile_y = y / tile_size;

      // Use the sum of tile coordinates to pick a grayscale level.
      // This creates the diagonal alternating "checker" effect.
      int color_idx = (tile_x + tile_y) % num_levels;
      uint8_t gray_val = grayscales[color_idx];

      /* * PACKING LOGIC:
       * A single byte in the SSD1322 RAM looks like this: [PX1_Nibble][PX2_Nibble]
       * Bit 7-4: First Pixel (Left)
       * Bit 3-0: Second Pixel (Right)
       */
      uint8_t packed_byte = (gray_val << 4) | (gray_val & 0x0F);
      buffer[buffer_idx++] = packed_byte;
    }
  }

  // DATA TRANSMISSION
  // Send the entire generated buffer over SPI.
  // We use chunked sending to avoid overflowing small SPI DMA buffers (usually 4KB).
  this->send_buffer_chunked(buffer, buffer_size, 2048);

  // CLEANUP
  free(buffer);
}
