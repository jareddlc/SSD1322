#include "ssd1322.h"

#ifdef SSD1322_HAS_LVGL

extern "C" {
  void IRAM_ATTR ssd1322_lvgl_post_cb(spi_transaction_t *t) {
    ssd1322_trans_config_t *conf = (ssd1322_trans_config_t*)t->user;
    if (conf != nullptr && conf->lv_display != nullptr) {
      lv_display_flush_ready((lv_display_t *)conf->lv_display);
    }
  }
}

void IRAM_ATTR SSD1322::lvgl_align_area(lv_event_t *e) {
  lv_area_t *area = (lv_area_t *)lv_event_get_param(e);

  // SSD1322 columns are bytes representing 2 pixels
  // but address increments are often tied to 4-pixel boundaries
  area->x1 &= ~3;                       // align x1 down to the nearest multiple of 4
  area->x2 = ((area->x2 + 4) & ~3) - 1; // align x2 up to the nearest multiple of 4 then subtract 1 to keep the width a multiple of 4
}

void IRAM_ATTR SSD1322::lvgl_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map, uint8_t *pixel_buff) {
  // calculate the number of pixels in the area
  int32_t width = area->x2 - area->x1 + 1;
  int32_t height = area->y2 - area->y1 + 1;
  int32_t num_pixels = width * height;
  // uint8_t *buf = px_map;
  // uint16_t pixels = ((area->x2 - area->x1) + 1) * ((area->y2 - area->y1) + 1); // why +1?
  // uint16_t bytes = pixels >> 1;

  // for (uint16_t x = 0; x < pixels; x++) {
  //   uint16_t z = x >> 1;              // each two pixels go into one byte
  //   uint8_t pixel_4bit = buf[x] >> 4; // convert 8-bit  to 4-bit

  //   // if x is odd
  //   if (x & 1u) {
  //     pixel_buff[z] |= pixel_4bit;        // store lower 4 bits
  //   } else {
  //     pixel_buff[z] = (pixel_4bit << 4);  // store upper 4 bits
  //   }
  // }

  // convert 8-bit px_map to 4-bit pixel_buff. SSD1322 packs two 4-bit pixels into one byte
  for (int32_t i = 0; i < num_pixels; i += 2) {
      // take two 8-bit pixels (0-255), shift them to 4-bit (0-15), and combine
      uint8_t high = px_map[i] >> 4;     // high nibble
      uint8_t low = px_map[i + 1] >> 4; // low nibble
      pixel_buff[i / 2] = (high << 4) | (low & 0x0F);
  }

  // set the window on the SSD1322
  uint8_t start_col = this->col_offset + (area->x1 / 4);
  uint8_t end_col   = this->col_offset + (area->x2 / 4);
  this->set_column_address(start_col, end_col);
  this->set_row_address(area->y1, area->y2);
  this->set_write_ram();

  // check for async/sync mode and send the packed buffer over SPI
  if (this->queue_size > 1) {
    this->send_buffer_async(pixel_buff, num_pixels / 2, display);
  } else {
    this->send_buffer(pixel_buff, num_pixels / 2);
    lv_display_flush_ready(display);
  }
}

#endif
