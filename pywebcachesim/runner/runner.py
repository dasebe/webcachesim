import multiprocessing
import tqdm
from pywebcachesim.simulation import simulation
from pywebcachesim.runner import parser
import timeit
from pymongo import MongoClient


def run_task(args):
    scheduler_args, task, worker_extra_args = args
    start_time = timeit.default_timer()
    params = {}
    for k, v in task.items():
        if k not in ['trace_file', 'cache_type', 'cache_size']:
            params[k] = str(v)
    for k, v in worker_extra_args.items():
        if v is not None:
            params[k] = str(v)
    res = simulation(f'{scheduler_args["trace_dir"]}/{task["trace_file"]}',
                     task['cache_type'],
                     task['cache_size'],
                     params)
    elapsed = timeit.default_timer() - start_time
    print(f'time for trace_file {task["trace_file"]}, icache_type {task["cache_type"]}, '
          f'cache_size {task["cache_size"]}: {elapsed} second')
    # print(res)
    if scheduler_args.get("dburl") is not None:
        print('writing result back...')
        _res = {
            'byte_hit_rate': float(res['byte_hit_rate']),
            'object_hit_rate': float(res['object_hit_rate']),
            'simulation_time': elapsed,
        }
        if "segment_byte_hit_rate" in res:
            _res["segment_byte_hit_rate"] = [float(r) for r in res['segment_byte_hit_rate'].split()]
        if "segment_object_hit_rate" in res:
            _res["segment_object_hit_rate"] = [float(r) for r in res['segment_object_hit_rate'].split()]
        record = {
            "res": _res,
            "scheduler_args": scheduler_args,
            "worker_extra_args": worker_extra_args,
            "task": task,
        }
        uri = (f'mongodb://{scheduler_args["dbuser"]}:{scheduler_args["dbpassword"]}@{scheduler_args["dburl"]}:'
               f'{scheduler_args["dbport"]}/{scheduler_args["dbname"]}')
        client = MongoClient(uri)
        db = client.get_database()
        collection = db[scheduler_args["dbcollection"]]
        # todo: the task must sorted in unique order in order to make db unique key
        collection.replace_one({'task': record["task"]}, record, upsert=True)

    return res


def runner_run(scheduler_args: dict, tasks: list, worker_extra_args: dict):
    # seq the task
    for i, task in enumerate(tasks):
        tasks[i] = (scheduler_args, task, {'task_id': i, **worker_extra_args})
    # debug mode, disable parallel
    if scheduler_args["debug"]:
        return [run_task(tasks[0])]
    else:
        # normal mode
        # use less CPU, otherwise some task may stuck in the middle,  causing the job not return
        with multiprocessing.Pool(multiprocessing.cpu_count() - 4) as pool:
            for _ in tqdm.tqdm(pool.imap_unordered(run_task, tasks), total=len(tasks)):
                pass


def main():
    scheduler_args, tasks, worker_extra_args = parser.parse()
    return runner_run(scheduler_args, tasks, worker_extra_args)


if __name__ == '__main__':
    main()
