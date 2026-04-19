idf_component_register(
    SRCS "ssd1322.cpp" "ssd1322_lvgl.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_spi freertos lvgl__lvgl
)

# force the definition since we are explicitly requiring LVGL above
target_compile_definitions(${COMPONENT_LIB} PUBLIC SSD1322_HAS_LVGL)
