import yaml
import pandas as pd
from pymongo import MongoClient

log_dir = '/data/zhenyus/webcachesim/log'

scheduler_args = {
  "dburl": "ds135724.mlab.com",
  "dbport": "35724",
  "dbname": "webcachesim",
  "dbuser": "zhenyus",
  "dbpassword": "szy123456",
  "dbcollection": "development",
}

def to_label(x: dict):
    """
    what to put on legend
    todo: refine the flow
    """
    return yaml.dump(
        {k: x[k] for k, v in x.items() if k not in {
            'cache_size',
            'trace_file', 
            'n_warmup', 
            'n_early_stop',
            'uni_size',
        }})
    
def load_reports(log_dir):
    uri = (f'mongodb://{scheduler_args["dbuser"]}:{scheduler_args["dbpassword"]}@{scheduler_args["dburl"]}:'
           f'{scheduler_args["dbport"]}/{scheduler_args["dbname"]}')
    client = MongoClient(uri)
    db = client.get_database()
    collection = db[scheduler_args["dbcollection"]]

    rows = []
    for r in collection.find():
        row = {}
        row.update(r['res'])
        row.update(r['task'])
        row.update(r['worker_extra_args'])
        row['label'] = to_label(r['task'])
        rows.append(row)
    df = pd.DataFrame(rows)
    return df
