idf_component_register(
    SRCS "wireless.c"
    INCLUDE_DIRS "."
    REQUIRES json
    PRIV_REQUIRES esp_http_client mqtt nvs_flash
)