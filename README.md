# GNDCTRL Weather Station
====================

An amateur-meteorologist weather and air quality logging station. It reads weather data from environmental sensors and sends the data to be logged in a database on a server.

# Hardware
The weather station is based on the Espressif ESP32. On board is a DS3231 for timekeeping, and a microSD card connected over SPI to log data and save configuration files.

Currently, the weather station uses the Bosch BME280 to measure temperature, barometric pressure, and relative humidity (and calculates dew point). It uses a PLANTOWER PMS5003 to measure PM1.0, PM2.5, PM10 and to count particles 0.3, 0.5, 1.0, 2.5, 5.0, and 10 microns diameter per deciliter.

# Software
The weather station can be quickly reconfigured to work with any environmental sensor by writing a class that inherits from the `Sensor` class. Then you just add your new sensor class to the `sensors` array in `main.cpp`.

Every fifth minute (i.e. 12:00, 12:05, 12:10, and so on) data is read from each sensor and sent via MQTT to the server.

 