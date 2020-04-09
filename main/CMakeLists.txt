# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "main.cpp"
				   "bsp.cpp" 
				   "services/serial/i2c.cpp"
				   "services/serial/uart.cpp"
				   "services/storage/sdcard.cpp"
				   "services/logger/logger.cpp"
				   "services/wireless/wifi.cpp"
				   "services/wireless/mqtt.cpp"
				   "hardware/ds3231/ds3231.cpp"
				   )

set(COMPONENT_ADD_INCLUDEDIRS "."
							  "services/serial/include"
							  "services/storage"
							  "services/logger"
							  "services/wireless/include"
							  "hardware"
							  "hardware/ds3231"
							  )

register_component()
