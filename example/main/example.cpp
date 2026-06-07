#include <stdio.h>
#include "ssd1322.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "lvgl.h"

extern "C" void IRAM_ATTR ssd1322_lvgl_post_cb(spi_transaction_t *t);

struct DisplayPins {
  static constexpr gpio_num_t CS   = GPIO_NUM_13;
  static constexpr gpio_num_t DC   = GPIO_NUM_9;
  static constexpr gpio_num_t RES  = GPIO_NUM_10;
  static constexpr gpio_num_t SCLK = GPIO_NUM_12;
  static constexpr gpio_num_t MOSI = GPIO_NUM_11;
};
static constexpr uint16_t SCREEN_WIDTH = 256;
static constexpr uint16_t SCREEN_HEIGHT = 64;

SSD1322 oled(DisplayPins::CS, DisplayPins::DC, DisplayPins::RES, DisplayPins::SCLK, DisplayPins::MOSI, SPI3_HOST);

extern "C" void app_main() {
  // init OLED asynchronously
  oled.init(SCREEN_WIDTH, SCREEN_HEIGHT, ssd1322_lvgl_post_cb);
  // or synchronously
  // oled.init(SCREEN_WIDTH, SCREEN_HEIGHT, nullptr);

  // run test if LVGL is not detected
  #ifndef SSD1322_HAS_LVGL
  printf("LVGL component not found. Running hardware test mode.\n");
  oled.test();
  return;
  #endif

  // init LVGL
  lv_init();

  // create buffer (half the screen for rendering buffer)
  const uint32_t disp_buff_size = (SCREEN_WIDTH * (SCREEN_HEIGHT >> 1));
  const uint32_t pixel_buff_size = disp_buff_size >> 1;

  // allocate DMA capable memory (alignment of 4 is standard for SPI DMA on ESP32)
  uint8_t* disp_buff_1 = (uint8_t *)heap_caps_aligned_alloc(4, disp_buff_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  uint8_t* disp_buff_2 = (uint8_t *)heap_caps_aligned_alloc(4, disp_buff_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  uint8_t* pixel_buff  = (uint8_t *)heap_caps_aligned_alloc(4, pixel_buff_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

  // configure display
  lv_display_t * disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_L8); // set color format, this is important
  lv_display_set_buffers(disp, disp_buff_1, disp_buff_2, disp_buff_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

  // store buffer in user_data
  lv_display_set_user_data(disp, pixel_buff);

  // set LVGL callbacks
  lv_display_set_flush_cb(disp, [](lv_display_t * d, const lv_area_t * area, uint8_t * px_map) {
    // get the buffer from user_data
    uint8_t* p_buff = (uint8_t*)lv_display_get_user_data(d);
    oled.lvgl_flush(d, area, px_map, p_buff);
  });
  lv_display_add_event_cb(disp, [](lv_event_t * e) {
    oled.lvgl_align_area(e);
  }, LV_EVENT_INVALIDATE_AREA, nullptr);

  // UI code
  lv_obj_t * screen = lv_screen_active();

  // set display background color and remove scrollbar since its not a touchscreen.
  lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_text_color(screen, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

  // create label
  lv_obj_t * label = lv_label_create(screen);
  lv_label_set_text(label, "SSD1322");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_center(label);

  // create bar
  lv_obj_t * bar = lv_bar_create(screen);
  lv_obj_set_size(bar, 256, 10);
  lv_obj_set_pos(bar, 0, 0);
  lv_bar_set_value(bar, 100, LV_ANIM_OFF);

  // create timer
  const esp_timer_create_args_t tick_timer_args = {
    .callback = [](void* arg) { lv_tick_inc(5); },
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "lvgl_tick",
    .skip_unhandled_events = false
  };
  esp_timer_handle_t tick_timer;
  esp_timer_create(&tick_timer_args, &tick_timer);
  esp_timer_start_periodic(tick_timer, 5000); // 5000 microseconds = 5ms

  uint8_t value = 0;
  while (true) {
    value++;
    if (value > 100) value = 0;
    lv_bar_set_value(bar, value, LV_ANIM_OFF);

    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
