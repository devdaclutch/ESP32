idf_component_register(SRCS "lab4_3.c"
                            "esp_hidd_prf_api.c"
                            "hid_dev.c"
                            "hid_device_le_prf.c"
                            "icm42670.c"
                    PRIV_REQUIRES bt nvs_flash esp_driver_gpio
                    PRIV_REQUIRES esp_driver_i2c
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
