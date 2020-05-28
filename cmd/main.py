import requests
import csv
from typing import Optional


def get_tz(city: Optional[str] = None) -> str:
    """Returns the POSIX TZ string for the specified city. If no city is specified, returns the POSIX TZ string for
    the current location."""
    if city is None:
        import sys
        # Get the timezone city, e.g. 'America/New_York'
        if sys.platform == 'linux' or sys.platform == 'linux2':
            with open('etc/timezone', 'r') as file:
                city = file.read()
        else:
            get_ip_info = requests.get('https://ipinfo.io/json')
            if get_ip_info.ok:
                city = get_ip_info.json()['timezone']
            else:
                raise ConnectionError('unable to get IP info')

    # Lookup the POSIX timezone string
    with open('zones.csv') as file:
        zones = csv.reader(file)
        for timezone, tz in zones:
            if timezone == city:
                return tz
        else:
            raise KeyError


def get_city(tz: str) -> str:
    """Returns the city for the specified POSIX TZ string."""
    # Lookup the POSIX timezone string
    if tz is None:
        raise TypeError
    with open('zones.csv') as file:
        zones = csv.reader(file)
        for timezone, posix in zones:
            if posix == tz:
                return timezone
        else:
            raise KeyError


PRODUCTION = '192.168.0.21'
TEST = '192.168.0.105'

DELETE, NONE, ERROR, WARN, INFO, DEBUG, VERBOSE = list(range(-1, 6))

if __name__ == "__main__":
    with requests.Session() as s:
        ip = PRODUCTION

        # r = requests.head(f'http://{ip}/events', params={'main': -1, 'hello': 2})
        #r = requests.get(f'http://{ip}/tz')

        r = s.post(f'http://{ip}/restart')
        if not r.ok:
            print(r.text)

        #r = s.get(f'http://{ip}/tz')
        #if r.ok:
        #    print(get_city(r.text))



        r = s.put(f'http://{ip}/logging', params={'uart': WARN})
        if not r.ok:
            print(r.text)

        r = s.get(f'http://{ip}/logging')
        if not r.ok:
            print(r.text)
        else:
            print(r.json())

        r = s.delete(f'http://{ip}/events')
        if not r.ok:
            print(r.text)
            """
            r = s.put(f'http://{ip}/tz', data=get_tz())
            if not r.ok:
                print(r.text)

            r = s.get(f'http://{ip}/tz')
            print(get_city(r.text))

            r = s.put(f'http://{ip}/topic', data='test/test')
            if not r.ok:
                print(r.text)

            r = s.get(f'http://{ip}/topic')
            print(r.text)

            r = s.put(f'http://{ip}/mqtt', data='mqtt://192.168.1.2')
            if not r.ok:
                print(r.text)

            r = s.get(f'http://{ip}/mqtt')
            print(r.text)
            """

            #r = s.get(f'http://{ip}/events')
            #if r.ok:
            #    print(r.text)

            #print(r.raw.version)
            #print(r.status_code)
            #print(r.headers)
            #print(r.content)

            #r = s.get(f'http://{ip}/topic')  # , data='mqtt://192.168.0.2')
            #print(r.raw.version)
            #print(r.status_code)
            #rint(r.headers)
            #print(r.content)


