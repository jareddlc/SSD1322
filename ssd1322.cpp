#include <ssd1322.h>
#include <stdio.h>

constexpr int MAX_TRANSACTIONS = 2;
static DRAM_ATTR spi_transaction_t transactions[MAX_TRANSACTIONS] = {0};
static DRAM_ATTR uint16_t transaction_index = 0;

static void IRAM_ATTR spi_post_cb(spi_transaction_t *transaction) {
  if (transaction->user == nullptr) {
    return;
  }
  lv_display_flush_ready((lv_display_t *)transaction->user);
}

// cs - SPI chip select
// dc - SPI data/command
// reset - Reset
// sclk - SPI Clock
// sdin - SPI MOSI
// spi_host - ESP32 SPI host (SPI_HOST = 0 [SPI1], HSPI_HOST = 1 [SPI2], VSPI_HOST = 2 [SPI3])
// async - Flag to use async SPI
SSD1322::SSD1322(int cs, int dc, int reset, int sclk, int sdin, int spi_host, bool async) {
  this->cs = (gpio_num_t)cs;
  this->dc = (gpio_num_t)dc;
  this->reset = (gpio_num_t)reset;
  this->sclk = (gpio_num_t)sclk;
  this->sdin = (gpio_num_t)sdin;
  this->spi_host = spi_host;
  this->async = async;
}

void SSD1322::send_spi_transaction(int mode, const uint8_t *data, size_t length) {
  gpio_set_level(this->dc, mode);
  spi_transaction_t spi_transaction;
  spi_transaction.length = length * 8;
  spi_transaction.tx_buffer = data;
  spi_transaction.rxlength = 0;
  spi_transaction.rx_buffer = 0;
  spi_transaction.addr = 0;
  spi_transaction.cmd = 0;
  spi_transaction.flags = 0;

  spi_device_polling_transmit(this->spi, &spi_transaction);
  // spi_device_transmit(this->spi, &spi_transaction);
}

void IRAM_ATTR SSD1322::send_spi_transaction_async(uint8_t mode, const uint8_t *data, size_t length, void *display) {
  spi_transaction_t *transaction = &transactions[transaction_index];
  transaction_index = (transaction_index + 1) % MAX_TRANSACTIONS;

  // spi_transaction_t *transaction = (spi_transaction_t*)heap_caps_malloc(sizeof(spi_transaction_t), MALLOC_CAP_DMA);
  // memset(transaction, 0, sizeof(spi_transaction_t));

  transaction->length = length * 8;
  transaction->rxlength = 0;
  transaction->user = (void *)display;
  transaction->tx_buffer = data;

  gpio_set_level(this->dc, mode);
  spi_device_queue_trans(this->spi, transaction, portMAX_DELAY);
}

void SSD1322::send_ssd1322_command(unsigned char d) {
  this->send_spi_transaction(0, &d, 1);
}

void SSD1322::send_ssd1322_data(unsigned char d) {
  this->send_spi_transaction(1, &d, 1);
}

void SSD1322::send_ssd1322_data_buffer(const uint8_t *data, size_t length) {
  this->send_spi_transaction(1, data, length);
}

void SSD1322::send_ssd1322_data_buffer_async(const uint8_t *data, size_t length, lv_display_t *disp) {
  this->send_spi_transaction_async(1, data, length, disp);
}

void SSD1322::send_ssd1322_data_buffer_chunked(uint8_t *data, size_t length, size_t chunk_size) {
  for (size_t offset = 0; offset < length; offset += chunk_size) {
    size_t len = (offset + chunk_size <= length) ? chunk_size : (length - offset);
    this->send_ssd1322_data_buffer(data + offset, len);
  }
}

void SSD1322::send_ssd1322_data_buffer_chunked_async(uint8_t *data, size_t length, size_t chunk_size, lv_display_t *disp) {
  for (size_t offset = 0; offset < length; offset += chunk_size) {
    size_t len = (offset + chunk_size <= length) ? chunk_size : (length - offset);
    this->send_ssd1322_data_buffer_async(data + offset, len, disp);
  }
}

void SSD1322::init(int columns, int rows) {
  // set oled size
  this->columns = columns;
  this->rows = rows;

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
  // buscfg.max_transfer_sz = 8192; // for testing

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
  devcfg.pre_cb = nullptr;
  devcfg.post_cb = nullptr;
  devcfg.queue_size = MAX_TRANSACTIONS;
  if (this->async == true) {
    devcfg.post_cb = spi_post_cb;
  }

  // init SPI
  spi_bus_initialize((spi_host_device_t)this->spi_host, &buscfg, SPI_DMA_CH_AUTO);
  spi_bus_add_device((spi_host_device_t)this->spi_host, &devcfg, &this->spi);

  this->reset_device();

  // initialization sequence
  this->set_command_lock(this->COMMANDS_UNLOCK);
  this->set_display_on_off(this->DISPLAY_OFF);
  this->set_front_clock_divider(0x91); // set clock as 80 frames/sec
  this->set_multiplex_ratio(0x3F);     // 1/64 duty (0x0F~0x3F)
  this->set_display_offset(0x00);      // shift mapping ram counter (0x00~0x3F)
  this->set_display_start_line(0x00);
  // this->set_function_selection(0x01); // enable internal VDD regulator
  this->set_remap_dual_com_line_mode(0x14);
  this->set_function_selection(0x01); // enable internal VDD regulator
  this->set_display_enhancement_a(0xA0, 0xFD);
  this->set_contrast_current(0x9F);
  this->set_master_current_control(0x0F);
  this->set_default_linear_gray_scale_table();
  // this->set_contrast_current(0x9F);
  this->set_phase_length(0xE2); // phase 1 (reset) & phase 2 (pre-charge) period adjustment
  // this->set_phase_length(0xF2);      // phase 1 (reset) & phase 2 (pre-charge) period adjustment (NHD-2.7-12864WDW3-M datasheet)
  this->set_precharge_voltage(0x1F); // 0.6*VCC
  // this->set_display_enhancement_a(0xA0, 0xFD);
  this->set_vcomh_voltage(0x07); // 0.86*VCC (0x07) (0x04)
  this->set_display_mode(this->DISPLAY_MODE_NORMAL);
  this->set_exit_partial_display();

  // clear ram
  this->fill_ram(0x00);
  // turn on display
  this->set_display_on_off(this->DISPLAY_ON);

  vTaskDelay(pdMS_TO_TICKS(100));
}

void SSD1322::reset_device() {
  // to reset set RESET pin to logic LOW for at least 100us
  // and then logic HIGH (fom SSD1322 manual)
  gpio_set_level(this->reset, 0);
  vTaskDelay(pdMS_TO_TICKS(300));
  gpio_set_level(this->reset, 1);
  vTaskDelay(pdMS_TO_TICKS(300));
}

// | Resolution | Columns (bytes) | Pixels | Column Address Range | Row Address Range |
// |------------|-----------------|--------|----------------------|-------------------|
// | 256 × 64   | 128             | 256    | 0x00 to 0x7F         | 0x00 to 0x3F      |
// | 480 × 128  | 240             | 480    | 0x00 to 0xEF         | 0x00 to 0x7F      |
void SSD1322::fill_ram_480_128(unsigned char d) {
  // the SSD1322 is a 4-bit grayscale display with support up to 480 x 128.
  // each byte in RAM holds 2 horizontal pixels (because each pixel is 4 bits).
  // 480 / 2 = 240 bytes per row.
  this->set_column_address(0x00, 0xEF); // 0xEF = 239 => 240 bytes => 480 pixels
  this->set_row_address(0x00, 0x7F);    // 0x7F = 127 => 128 rows
  this->set_write_ram();

  for (unsigned char row = 0; row < 128; row++) {
    for (unsigned char col = 0; col < 240; col++) {
      // 1 byte = 2 pixels
      this->send_ssd1322_data(d);
    }
  }
}

void SSD1322::fill_ram(unsigned char d) {
  // funciont to fill 256 x 64 pixels.
  // each byte holds 2 horizontal pixels => 256 / 2 = 128 bytes per row.
  this->set_column_address(0x00, 0x7F); // 0x7F = 127 => 128 bytes => 256 pixels
  this->set_row_address(0x00, 0x3F);    // 0x3F = 63 => 64 rows
  this->set_write_ram();

  for (unsigned char row = 0; row < 64; row++) {
    for (unsigned char col = 0; col < 128; col++) {
      this->send_ssd1322_data(d);  // 1 byte = 2 pixels
    }
  }
}

void SSD1322::set_column_address(unsigned char d, unsigned char e) {
  this->send_ssd1322_command(this->SET_COLUMN_ADDRESS);
  this->send_ssd1322_data(d); // default => 0x00
  this->send_ssd1322_data(e); // default => 0x77
}

void SSD1322::set_write_ram() {
  this->send_ssd1322_command(this->WRITE_RAM);
}

void SSD1322::set_row_address(unsigned char d, unsigned char e) {
  this->send_ssd1322_command(this->SET_ROW_ADDRESS);
  this->send_ssd1322_data(d); // default => 0x00
  this->send_ssd1322_data(e); // default => 0x7F
}

void SSD1322::set_remap_dual_com_line_mode(unsigned char d) {
  this->send_ssd1322_command(this->SET_REMAP_DUAL_COM_LINE_MODE);
  this->send_ssd1322_data(d);    // 0x14 NORMAL, 0x06 FLIP?, 0x16?
  this->send_ssd1322_data(0x11); // default => 0x01 (Disable Dual COM Mode)
}

void SSD1322::set_display_start_line(unsigned char d) {
  this->send_ssd1322_command(this->SET_DISPLAY_START_LINE);
  this->send_ssd1322_data(d);
}

void SSD1322::set_display_offset(unsigned char d) {
  this->send_ssd1322_command(this->SET_DISPLAY_OFFSET);
  this->send_ssd1322_data(d);
}

void SSD1322::set_display_mode(unsigned char d) {
  this->send_ssd1322_command(d);
}

void SSD1322::set_exit_partial_display() {
  this->send_ssd1322_command(this->EXIT_PARTIAL_DISPLAY);
}

void SSD1322::set_function_selection(unsigned char d) {
  this->send_ssd1322_command(this->SET_FUNCTION_SELECTION);
  this->send_ssd1322_data(d);
}

void SSD1322::set_display_on_off(unsigned char d) {
  this->send_ssd1322_command(d);
}

void SSD1322::set_phase_length(unsigned char d) {
  this->send_ssd1322_command(this->SET_PHASE_LENGTH);
  this->send_ssd1322_data(d);
}

void SSD1322::set_front_clock_divider(unsigned char d) {
  this->send_ssd1322_command(this->SET_FRONT_CLOCK_DIVIDER);
  this->send_ssd1322_data(d);
}

void SSD1322::set_display_enhancement_a(unsigned char d, unsigned char e) {
  this->send_ssd1322_command(this->DISPLAY_ENHANCEMENT_A);
  this->send_ssd1322_data(d);
  this->send_ssd1322_data(e);
}

void SSD1322::set_default_linear_gray_scale_table() {
  this->send_ssd1322_command(this->SELECT_DEFAULT_LINEAR_GRAY_SCALE_TABLE);
}

void SSD1322::set_precharge_voltage(unsigned char d) {
  this->send_ssd1322_command(this->SET_PRECHARGE_VOLTAGE);
  this->send_ssd1322_data(d);
}

void SSD1322::set_vcomh_voltage(unsigned char d) {
  this->send_ssd1322_command(this->SET_VCOMH_VOLTAGE);
  this->send_ssd1322_data(d);
}

void SSD1322::set_contrast_current(unsigned char d) {
  this->send_ssd1322_command(this->SET_CONTRAST_CURRENT);
  this->send_ssd1322_data(d);
}

void SSD1322::set_master_current_control(unsigned char d) {
  this->send_ssd1322_command(this->MASTER_CURRENT_CONTROL);
  this->send_ssd1322_data(d);
}

void SSD1322::set_multiplex_ratio(unsigned char d) {
  this->send_ssd1322_command(this->SET_MULTIPLEX_RATIO);
  this->send_ssd1322_data(d);
}

void SSD1322::set_command_lock(unsigned char d) {
  this->send_ssd1322_command(this->SET_COMMAND_LOCK);
  this->send_ssd1322_data(d);
}

void SSD1322::test() {
  // SSD1322 - 16 gray scale levels supported by embedded 480 x 128 x 4 bit SRAM display buffer
  // the screen has 256x64 pixels, but each pixel is 4 bits wide.
  uint8_t col_start = 0;
  uint8_t col_end = this->columns - 1;
  uint8_t row_start = 0;
  uint8_t row_end = this->rows - 1;

  size_t buffer_size = (this->columns * this->rows) / 2;
  uint8_t* buffer = (uint8_t*)malloc(buffer_size);

  // SSD1322 column addressing basics:
  // each RAM column (1 byte) = 2 horizontal pixels, each 4 bits wide.
  // the SSD1322 has 480 columns of pixels, which equals 240 bytes of column RAM.
  // but the SSD1322 internally maps RAM columns 0x00 to 0x77 (119 bytes) to the left side of the screen, not the full width.
  // so to center or align a smaller display like 256×64 in the full 480×128 canvas, you often need to add an offset.

  // 0x1C is decimal 28.
  // this offset shifts your drawing window to align your logical display with the center of the physical screen.
  // it's a commonly recommended value in the SSD1322 datasheet and initialization examples for 256-pixel-wide displays.

  // so the set_column_address below, maps your logical column 0 (pixel 0) to RAM column 28, which starts drawing more centrally on the actual display.

  // SD1322 RAM:     0  1  2  ...  28  ...  ...  127  ...  238  239
  // physical px:   <- unused -> [Your 256x64 display] <- unused ->
  //                             ↑ start drawing here (offset = 0x1C)

  // set address window (SSD1322 expects byte-based column addressing: divide by 2)
  this->set_column_address(0x1C + (col_start / 4), 0x1C + (col_end / 4));
  this->set_row_address(row_start, row_end);
  this->set_write_ram();

  const int tile_size = 8;
  const uint8_t grayscales[] = { 0x0, 0x5, 0xA, 0xF };
  const int num_grayscales = sizeof(grayscales) / sizeof(grayscales[0]);

  int index = 0;
  for (int row = row_start; row <= row_end; row++) {
    for (int col = col_start; col <= col_end; col += 2) {
      // determine tile position
      int tile_x = col / tile_size;
      int tile_y = row / tile_size;

      // use XOR of tile coords to alternate pattern
      int checker_index = (tile_x + tile_y) % num_grayscales;
      uint8_t gray = grayscales[checker_index];

      // pack two grayscale pixels into one byte
      uint8_t pixel_byte = (gray << 4) | gray;
      buffer[index++] = pixel_byte;
    }
  }

  this->send_ssd1322_data_buffer_chunked(buffer, buffer_size, 2048);
  free(buffer);
}
