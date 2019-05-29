import pandas as pd
import numpy as np
from pymongo import MongoClient

WEBCACHESIM_ROOT = '/data/zhenyus/webcachesim'

scheduler_args = {
    "dburl": "ec2-34-237-51-183.compute-1.amazonaws.com",
    "dbport": "27017",
    "dbname": "webcachesim",
    "dbuser": "zhenyus",
    "dbpassword": "szy123456",
    "dbcollection": "dev",
}


def to_label(x: dict):
    """
    what to put on legend
    """
    return ' '.join([
        f'{k}: {v}' for k, v in x.items() if k not in {
            'cache_size',
            'trace_file',
            'simulation_time',
            'n_warmup',
            'n_early_stop',
            'uni_size',
            'byte_miss_ratio',
            'object_miss_ratio',
            'segment_byte_miss_ratio',
            'segment_object_miss_ratio',
            'miss_decouple',
            'cache_size_decouple',
        }
    ])

def load_reports(dbcollection=None):
    uri = (f'mongodb://{scheduler_args["dbuser"]}:{scheduler_args["dbpassword"]}@{scheduler_args["dburl"]}:'
           f'{scheduler_args["dbport"]}')
    client = MongoClient(uri)
    db = client.get_database("webcachesim")
    if dbcollection is None:
        collection = db[scheduler_args["dbcollection"]]
    else:
        collection = db[dbcollection]

    rows = []
    for r in collection.find():
        row = {k:v for k,v in r.items() if k != '_id'}
        for k in ['cache_size', 'uni_size', 'n_warmup', 'byte_miss_ratio', 'object_miss_ratio']:
            if k in row:
                try:
                    row[k] = float(row[k])
                except Exception as e:
                    row[k] = np.nan
        row['label'] = to_label(row)
        rows.append(row)
    df = pd.DataFrame(rows)
    return df

