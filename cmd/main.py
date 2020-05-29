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
            raise KeyError(f'no such timezone for: {tz}')


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
# DONE get tz, put tz
# post restart


class UpdateLoggingDict(argparse.Action):
    def __init__(self, option_strings, dest, nargs, **kwargs):
        super(UpdateLoggingDict, self).__init__(option_strings, dest, **kwargs)

    def __call__(self, _, namespace, values, option_string=None):
        d = {}
        for tag in values.split():
            d[tag] = self.const
        if (out := getattr(namespace, self.dest)) is None:
            setattr(namespace, self.dest, d)
        else:
            out.update(d)


parser = argparse.ArgumentParser(description='Control the weather station')
subparser = parser.add_subparsers(dest='command')

# Configure the target argument
parser.add_argument('-t', dest='target', metavar='host', action='store', type=str, nargs='?',
                    help='set the target device')
parser.add_argument('-s', dest='save_target', action='store_true',
                    help='save the specified target for future commands')

# Configure the restart argument
parser.add_argument('--restart', dest='restart', action='store_true', default=False,
                    help='restart the device')

# Configure the parser for parsing logging level arguments
log_parser = subparser.add_parser('log', help='manage the log and logging levels')
log_levels = ['none', 'error', 'warning', 'info', 'debug', 'verbose']
for i, level in sorted(enumerate(log_levels), key=lambda key: key[0], reverse=True):
    log_parser.add_argument(f'-{level[0]}', dest='logging', metavar='tag',
                            action=UpdateLoggingDict, type=str, const=i, nargs='+',
                            help=f'set \'tag\' to {level} in the log file')
log_parser.add_argument('-rm', dest='logging', metavar='tag',
                        action=UpdateLoggingDict, type=str, const=-1, nargs='+',
                        help='logging tags to remove from the log file')
log_parser.add_argument('-l', '--list', dest='list_logging', action='store_true',
                        help='list the logging levels')

# Configure the parser for parsing events
log_parser.add_argument('-s', '--size', dest='get_size', action='store_true',
                        help='get the size of the events log file')
log_parser.add_argument('--get', dest='get_events', action='store_true',
                        help='show the events log in the shell')
log_parser.add_argument('--clear', dest='delete_events', action='store_true',
                        help='delete the events file')

# Configure the parser for getting and setting the MQTT broker and topic
mqtt_parser = subparser.add_parser('mqtt', help='get or set the MQTT broker and topic')
mqtt_parser.add_argument('-t', '--topic', dest='topic', action='store', nargs='?', type=str,
                         default=False, help='view the MQTT topic name or specify a topic to set '
                                             'it')
mqtt_parser.add_argument('-b', '--broker', dest='broker', action='store', nargs='?', type=str,
                         default=False, help='view the MQTT broker name or specify a name to set '
                                             'it')

# Configure the parser for getting, setting, and listing the timezone
tz_parser = subparser.add_parser('tz', help='manage the device timezone')
tz_parser.add_argument('--get', dest='tz_get', action='store_true', default=False,
                       help='get the configured timezone')
tz_parser.add_argument('--set', dest='tz_set', metavar='tz', action='store', default=False,
                       type=str, nargs='?', help='set the configured timezone')
tz_parser.add_argument('--list', dest='tz_list', action='store_true', default=False,
                       help='list the known timezones')

if __name__ == "__main__":
    args = parser.parse_args()

    if args.target is None:
        with open('target', 'r') as target_file:
            args.target = target_file.read()

    elif args.save_target:
        with open('target', 'w') as target_file:
            target_file.write(args.target)

    with requests.Session() as s:
        if args.restart:
            r = s.post(f'http://{args.target}/restart')
            if not r.ok:
                raise ConnectionError(r.text)

        if args.command == 'log':
            # Handle logging level requests
            if args.logging is not None:
                r = s.put(f'http://{args.target}/logging', params=args.logging)
                if warnings := r.headers.get('Warning'):
                    for warning in warnings:
                        print(warning.split()[-1])
                if not r.ok:
                    raise ConnectionError(r.text)

            if args.list_logging:
                levels = ['None', 'Error', 'Warning', 'Info', 'Debug', 'Verbose']
                r = s.get(f'http://{args.target}/logging')
                if r.ok:
                    j = r.json()
                    tabs = len(max(j.keys(), key=len))
                    print('Log levels:')
                    for key, value in sorted(j.items()):
                        print(f'    {key:<{tabs}}   {levels[value]}')

                else:
                    print(f'Error: {r.text}')

            # Handle event log requests
            if args.get_events:
                r = s.get(f'http://{args.target}/events')
                if not r.ok:
                    raise ConnectionError(r.text)
                else:
                    print(r.text)
                if args.get_size:
                    print(r.headers['Content-Length'])
            elif args.get_size:
                r = s.get(f'http://{args.target}/events')
                if not r.ok:
                    raise ConnectionError(r.text)
                else:
                    print(r.headers['Content-Length'])
            if args.delete_events:
                r = s.delete(f'http://{args.target}/events')
                if not r.ok:
                    raise ConnectionError(r.text)

        elif args.command == 'mqtt':
            to_print = []
            # Handle getting and setting the MQTT broker
            if args.broker is None:
                r = s.get(f'http://{args.target}/mqtt')
                if not r.ok:
                    raise ConnectionError(r.text)
                else:
                    to_print.append(f'Broker: {r.text}')
            elif args.broker:
                r = s.put(f'http://{args.target}/mqtt', data=args.broker)
                if not r.ok:
                    raise ConnectionError(r.text)

            # Handle getting and setting the MQTT topic
            if args.topic is None:
                r = s.get(f'http://{args.target}/topic')
                if not r.ok:
                    raise ConnectionError(r.text)
                else:
                    to_print.append(f'Topic: {r.text}')
            elif args.topic:
                r = s.put(f'http://{args.target}/topic', data=args.topic)
                if not r.ok:
                    raise ConnectionError(r.text)

            # Print results
            print(', '.join(to_print))

        # Handle timezones
        if args.command == 'tz':
            if args.tz_set is not False:
                r = s.put(f'http://{args.target}/tz', data=get_tz(args.tz_set))
                if not r.ok:
                    raise ConnectionError(r.text)
            if args.tz_get is True:
                r = s.get(f'http://{args.target}/tz')
                if not r.ok:
                    raise ConnectionError(r.text)
                else:
                    print(f'Timezone is: {get_tz(r.text)}')
            if args.tz_list is True:
                with open('zones.csv') as file:
                    zones = csv.reader(file)
                    print('Known timezones:')
                    for timezone, posix in zones:
                        print(f'    {timezone}')


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


