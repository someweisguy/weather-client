import requests
import csv
from typing import Optional, Dict
import argparse
import json


def get_tz(city: Optional[str] = None) -> str:
    """Returns the POSIX TZ string for the specified city. If no city is specified, returns the POSIX TZ string for
    the current location."""
    if city is None:
        import sys
        # Get the timezone city, e.g. 'America/New_York'
        if sys.platform == 'linux' or sys.platform == 'linux2':
            with open('/etc/timezone', 'r') as file:
                city = file.read().strip()
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

# get events, head events, delete events
# get mqtt, put mqtt
# get topic, put topic
# get tz, put tz
# post restart


def logging(ip: str, params: Optional[Dict[str, int]] = None, s: Optional[requests.Session] = None):
    """Get or set the logging levels. If 'params' is None, print the current log levels."""
    levels = ['None', 'Error', 'Warning', 'Info', 'Debug', 'Verbose']
    if s is None:
        s = requests

    if params is None:
        r = s.get(f'http://{ip}/logging')
        if r.ok:
            j = r.json()
            tabs = len(max(j.keys(), key=len))
            print('Log levels:')
            for key, value in sorted(j.items()):
                print(f'    {key:<{tabs}}   {levels[value]}')

        else:
            print(f'Error: {r.text}')
    else:
        r = s.put(f'http://{ip}/logging', params=params)
        if not r.ok:
            print(f'Error: {r.text}')
        elif r.headers.get('Warning'):
            pass  # TODO


def events(ip: str, delete_after: bool = False, s: Optional[requests.Session] = None):
    """Prints the events log. Deletes the events log afterwards if 'delete_after' is True."""
    if s is None:
        s = requests

    r = s.get(f'http://{ip}/events')
    if not r.ok:
        print(f'Error: {r.text}')
        return
    else:
        print(r.text)

    if delete_after:
        r = s.delete(f'http://{ip}/events')
        if not r.ok:
            print(f'Error: {r.text}')


def restart(ip: str, s: Optional[requests.Session] = None):
    if s is None:
        s = requests

    r = s.post(f'http://{ip}/restart')
    if not r.ok:
        print(f'Error: {r.text}')


class UpdateLoggingDict(argparse.Action):
    def __call__(self, _, namespace, values, option_string=None):
        d = {}
        for tag in values:
            d[tag] = self.const
        if (out := getattr(namespace, self.dest)) is None:
            setattr(namespace, self.dest, d)
        else:
            out.update(d)


parser = argparse.ArgumentParser(description='Control the weather station')

# Configure the parser for parsing logging level arguments
logging_levels = ['none', 'error', 'warning', 'info', 'debug', 'verbose']
for i, level in enumerate(logging_levels):
    parser.add_argument(f'-{level[0]}', dest='logging', metavar='tag', action=UpdateLoggingDict,
                        type=str, const=i, nargs='+', help=f'logging tags to set to {level} in the log file')
parser.add_argument('--del', dest='logging', metavar='tag', action=UpdateLoggingDict,
                    type=str, const=-1, nargs='+', help='logging tags to remove from the log file')
parser.add_argument('-l', '--list', action='store_true', help='list the logging levels')

# Configure the parser for parsing get and set TZ
parser.add_argument('--tz', dest='timezone', metavar='tz', action='store', type=str,
                    nargs='?', default=False, help='get or set the configured timezone')


if __name__ == "__main__":
    args = parser.parse_args()
    print(args)

    if args.logging is None:
        print('getting log levels')
    else:
        print('setting log levels')

        if args.list:
            print('listing the log levels')

    if args.timezone is False:
        print('getting timezone')
    elif args.timezone is not None:
        print('setting timezone')



    #logging(PRODUCTION)
    #events(PRODUCTION)
    #args = parser.parse_args()
    #print(args.accumulate(args.integers))
    """
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
            #print(r.content)"""


