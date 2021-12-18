import argparse
import socket
import time
import hashlib
from rich.progress import Progress, TimeElapsedColumn, TextColumn, BarColumn


MAX_LINE_SIZE = 8192
PROGRESS_UPDATE_TIME = 1


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument('wordlist')
    parser.add_argument('hashname')
    parser.add_argument('lookup', help='hostname:port of the lookup table')

    return parser.parse_args()


def dehash(lookup, hash):
    lookup.send(hash.encode('ascii'))
    result = lookup.recv(MAX_LINE_SIZE)

    if result == b'\n':
        return None

    return result[:-1]


def clean_line(line):
    tmp = line.find('\r')

    if tmp == -1:
        tmp = line.find('\n')

    return line[:tmp].encode('latin1')


if __name__ == '__main__':
    args = parse_args()
    lookup_hostname, lookup_port = args.lookup.split(':')
    wordlist_name = args.wordlist.split('/')[-1]
    lookup = socket.create_connection((lookup_hostname, lookup_port))
    found = 0
    error = 0
    total = 0
    last_total = 0
    ts = time.time()

    progressbar = Progress(
        TextColumn('Dehashing [bold]{task.description}'),
        TimeElapsedColumn(),
        BarColumn(),
        TextColumn('[green]{task.completed}[white] /[bold][red] {task.fields[error]} [/bold][white]/ {task.total} '
                   '({task.percentage:.2f}%) [white]- {task.fields[dehash_per_second]:.0f} dehash/s')
    )

    task = progressbar.add_task(f'{wordlist_name}', total=0, completed=0, error=0, dehash_per_second=0)
    progressbar.start()

    with open(args.wordlist, 'r', encoding='latin1') as wordlist:
        for line in wordlist:
            line = clean_line(line)
            h = hashlib.new(args.hashname)

            h.update(line)
            digest = h.hexdigest()

            try:
                dehashed = dehash(lookup, digest)
            except:
                line = line.decode('latin1')
                print(f'An error occurred while dehashing the word: {line}')
                exit(1)

            if dehashed is not None:
                if line == dehashed:
                    found += 1
                else:
                    error += 1

            total += 1
            current_ts = time.time()
            delta = current_ts - ts

            if delta >= PROGRESS_UPDATE_TIME:
                dehash_per_second = (total - last_total) / delta

                progressbar.update(task, completed=found, total=total, error=error, dehash_per_second=dehash_per_second)

                last_total = total
                ts = current_ts

    progressbar.update(task, completed=found, total=total, error=error, dehash_per_second=dehash_per_second)

    progressbar.stop()
    lookup.close()

    exit(0)
