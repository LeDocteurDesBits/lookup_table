import argparse
import socket
import time
import elasticsearch
import elasticsearch.helpers
from rich.progress import Progress, TimeElapsedColumn, TextColumn, BarColumn

import warnings
warnings.filterwarnings('ignore', category=elasticsearch.ElasticsearchDeprecationWarning)


MAX_LINE_SIZE = 8192
DOCUMENTS_BUFFER_SIZE = 500000
ELASTICSEARCH_TIMEOUT = 300


def search_not_dehashed_documents(elastic, index, size, timeout=ELASTICSEARCH_TIMEOUT):
    body = {
        "query": {
            "bool": {
                "must_not": {
                    "exists": {
                        "field": "password"
                    }
                }
            }
        }
    }

    return elastic.search(
        index=index,
        filter_path=['hits.hits._id', 'hits.hits._source'],
        size=size,
        request_timeout=timeout,
        body=body
    )['hits']['hits']


def update_dehashed_documents(elastic, index, documents):
    actions = [
        {
            '_op_type': 'update',
            '_index': index,
            '_type': '_doc',
            '_id': document['_id'],
            'doc': {
                'password': document['_source']['password']
            }
        }
        for document in documents if 'password' in document['_source']
    ]

    return elasticsearch.helpers.bulk(elastic, actions, stats_only=True)


def parse_args():
    parser = argparse.ArgumentParser(description='A tool to replace hashed password in an Elasticsearch database')

    parser.add_argument('elasticsearch', help='The URL where the Elasticsearch instance is reachable')
    parser.add_argument('index', help='The Elasticsearch index to analyse')
    parser.add_argument('lookup', help='hostname:port of the lookup table')

    return parser.parse_args()


def dehash(lookup, hash):
    lookup.send(hash.encode('ascii'))
    result = lookup.recv(MAX_LINE_SIZE)

    if result == b'\n':
        return None

    try:
        return result[:-1].decode('utf-8')
    except UnicodeDecodeError:
        return None


if __name__ == '__main__':
    args = parse_args()
    elastic = elasticsearch.Elasticsearch([args.elasticsearch])
    lookup_hostname, lookup_port = args.lookup.split(':')
    lookup = socket.create_connection((lookup_hostname, lookup_port))
    found = 0
    total = 0
    last_total = 0
    updated = 0
    not_updated = 0
    updated_percent = 0

    progressbar = Progress(
        TextColumn('Dehashing [bold]{task.description}'),
        TimeElapsedColumn(),
        BarColumn(),
        TextColumn('[green]{task.completed}[white] /[red] {task.fields[not_found]} [white]/ {task.total} '
                   '({task.percentage:.2f}%) - [green]{task.fields[updated]} [white]/ [red]{task.fields[not_updated]} '
                   '[white] ({task.fields[updated_percent]:.2f}%) - {task.fields[dehash_per_second]:.0f} dehash/s')
    )

    task = progressbar.add_task(f'{args.index}', total=0, not_found=0, updated=0, not_updated=0, updated_percent=0,
                                dehash_per_second=0)
    progressbar.start()

    try:
        while True:
            try:
                results = search_not_dehashed_documents(elastic, args.index, size=DOCUMENTS_BUFFER_SIZE)
            except elasticsearch.exceptions.ConnectionTimeout:
                print('Oh noes, Elasticsearch sucks')
                continue

            i = 0
            ts = time.time()

            for result in results:
                h = result['_source']['hash']
                plaintext = dehash(lookup, h)

                if plaintext is not None:
                    found += 1
                    result['_source']['password'] = plaintext

                total += 1

                current_ts = time.time()
                delta = current_ts - ts

                if delta >= 1:
                    dehash_per_second = (total - last_total) / delta

                    progressbar.update(task, completed=found, total=total,
                                       not_found=total - found, updated=updated, not_updated=not_updated,
                                       updated_percent=updated_percent, dehash_per_second=dehash_per_second)

                    last_total = total
                    ts = current_ts

            try:
                success, fail = update_dehashed_documents(elastic, args.index, results)

                updated += success
                not_updated += fail
                updated_percent = updated / (updated + not_updated) * 100
            except Exception as e:
                print(f'WOW, much Elasticsearch, so performance, much powerful: {e}')

    except KeyboardInterrupt:
        lookup.close()
        elastic.close()

        if total > 0:
            progressbar.update(task, completed=found, total=total, not_found=total - found, updated=updated,
                               not_updated=not_updated, updated_percent=updated_percent, dehash_per_second=0)

        progressbar.stop()

        print('Bye.')
        exit(0)

    lookup.close()
    elastic.close()

    exit(1)
