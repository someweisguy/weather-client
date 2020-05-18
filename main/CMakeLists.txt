# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "main.cpp"
				   "helpers.cpp"
				   "serial/i2c.cpp"
				   "serial/uart.cpp"
				   "wireless/wlan.cpp"
				   "wireless/http.cpp"
				   "wireless/mqtt.cpp"
				   "hardware/sdcard/sdcard.cpp"
				   "hardware/ds3231/ds3231.cpp"
				   #"services/power.cpp"
				   )

set(COMPONENT_ADD_INCLUDEDIRS "."
							  "serial/include"
							  "wireless/include"
							  "hardware"
							  "hardware/sdcard"
							  "hardware/ds3231"
							  #"services"
							  )

register_component()
