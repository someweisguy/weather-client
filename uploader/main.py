import glob
import json
import sys
from typing import Optional

import serial
import secrets


def list_serial_ports():
    """ Lists serial port names

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result


def upload_to_station(port: str, ssid: str, password: Optional[str]) -> Optional[str]:
    """ Uploads WiFi credentials to the station.

        :returns:
            An IP address string on success. None on failure.
    """
    try:
        payload = {'ssid': ssid, 'password': password}
        with serial.Serial(port=port, baudrate=115200) as esp:
            # Reset the board
            esp.dtr = False
            esp.rts = False
            esp.dtr = True
            esp.rts = True

            while True:
                try:
                    line = esp.readline().strip().decode()
                except UnicodeDecodeError:
                    continue

                if line.startswith('UPLOAD'):
                    _, step, *val = line.split()

                    # Handle JSON send/receive
                    if step.startswith('JSON'):
                        if step == 'JSON_READY':
                            print('Uploading WiFi credentials...', end=' ')
                            esp.write(json.dumps(payload).encode())
                        elif step == 'JSON_OK':
                            print('success!')
                        elif step == 'JSON_BAD_PAYLOAD':
                            print('failed. (bad payload)', file=sys.stderr)
                            break
                        elif step == 'JSON_UNEXPECTED_EVENT':
                            print('failed. (unexpected error)', file=sys.stderr)
                            break

                    # Handle WiFi connection
                    elif step.startswith('WIFI'):
                        if step == 'WIFI_READY':
                            print('Waiting for internet connection...', end=' ')
                        elif step == 'WIFI_OK':
                            print('success!')
                            return str(*val)
                        else:
                            print('failed (couldn\'t connect)', file=sys.stderr)
                            break

    except serial.serialutil.SerialException:
        print(f'Connection to {port} failed. Reconnect the hardware and try again.', file=sys.stderr)


def update_server(ip_address: str):
    pass


if __name__ == '__main__':
    # print(list_serial_ports())
    ip = upload_to_station('COM4', secrets.ssid, secrets.password)
    if ip is not None:
        print(f'Got IP address "{ip}".')
        update_server(ip)
