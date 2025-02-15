// This is an example of how to use this SSD1322 driver with LVGL and the ESP32 framework.
// The two important functions needed are ssd1322_lvgl_flush and ssd1322_lvgl_align_area.
// These functions are for telling LVGL how to write to the display correctly.

// SSD1322
#include "ssd1322.h"

// esp32
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_timer.h>

// graphics
#include <lvgl.h>

#define TAG "MAIN"
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 64
#define LV_TICK_PERIOD_MS 1

static SSD1322 oled{GPIO_NUM_5, GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_23, VSPI_HOST};

// display and buffer
static lv_disp_t *disp;
static constexpr uint32_t disp_buff_size = (SCREEN_WIDTH * (SCREEN_HEIGHT >> 3));
static uint8_t *disp_buf;
static constexpr uint32_t pixel_buff_size = disp_buff_size >> 1;
static uint8_t *pixel_buff;

// functions
static void lv_tick_task(void *arg) {
  (void)arg;
  lv_tick_inc(LV_TICK_PERIOD_MS);
}

void ssd1322_lvgl_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
  uint8_t* buf = px_map;
  uint16_t pixels = ((area->x2 - area->x1) + 1) * ((area->y2 - area->y1) + 1);
  uint16_t bytes = pixels >> 1;

  oled.set_column_address(0x1C + (area->x1 / 4), 0x1C + (area->x2 / 4));
  oled.set_row_address(area->y1, area->y2);
  oled.set_write_ram();

  for (uint16_t x = 0; x < pixels; x++) {
    uint16_t z = x >> 1; // Each two pixels go into one byte
    uint8_t pixel_4bit = buf[x] >> 4; // Conversion from 8-bit (0-255) to 4-bit (0-15)

    if (x & 1u) {
        pixel_buff[z] |= pixel_4bit;  // Store lower 4 bits
    } else {
        pixel_buff[z] = (pixel_4bit << 4);  // Store upper 4 bits
    }
  }

  oled.send_ssd1322_data_buffer(pixel_buff, bytes);
  lv_display_flush_ready(disp);
}

void ssd1322_lvgl_align_area(lv_event_t *e) {
  auto *area = (lv_area_t *) lv_event_get_param(e);

  area->x1 &= ~3;
  area->x2 = ((area->x2 + 4) & ~3) - 1;
}

void init_display() {
  // init OLED
  oled.init(SCREEN_WIDTH, SCREEN_HEIGHT);

  // init lvgl
  lv_init();

  // configure display and set render callbacks
  disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(disp, ssd1322_lvgl_flush);
  lv_display_add_event_cb(disp, ssd1322_lvgl_align_area, LV_EVENT_INVALIDATE_AREA, nullptr);

  disp_buf = (uint8_t *)heap_caps_aligned_alloc(4, disp_buff_size, MALLOC_CAP_8BIT);
  pixel_buff = (uint8_t *)heap_caps_aligned_alloc(4, pixel_buff_size, MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
  lv_display_set_buffers(disp, disp_buf, nullptr, disp_buff_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_L8); // IMPORTANT

  // create timer
  const esp_timer_create_args_t periodic_timer_args = {
      .callback = &lv_tick_task,
      .name = "periodic_gui"};
  esp_timer_handle_t periodic_timer;
  esp_timer_create(&periodic_timer_args, &periodic_timer);
  esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000);

  // set display background color and disable scrollbar.
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);
  lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);
}

// main
extern "C" [[noreturn]] void app_main(void) {
  // init and configure display
  init_display();

  // use lvgl here

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1));
    lv_timer_periodic_handler();
  }
}
