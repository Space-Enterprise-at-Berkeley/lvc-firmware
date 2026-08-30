list(APPEND COMPONENT_REQUIRES esp_netif espressif__esp_h264 esp_timer nvs_flash esp_eth)
list(APPEND COMPONENT_REQUIRES  esp_driver_ppa esp_hw_support esp_mm)
list(APPEND COMPONENT_REQUIRES esp_driver_dma esp_driver_parlio)

idf_component_register(
    SRCS "mpeg_ts.c" "stream.c" "main.c"
    lvb.c
    REQUIRES ${COMPONENT_REQUIRES}
    INCLUDE_DIRS ".")
