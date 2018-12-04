import multiprocessing
import tqdm
import arrow
import pathlib
from pywebcachesim.simulation import simulation
from pywebcachesim.runner import parser
import yaml
import timeit


def run_task(args):
    scheduler_args, task = args
    start_time = timeit.default_timer()
    params = {}
    for k, v in task.items():
        if k not in ['trace_file', 'cache_type', 'cache_size']:
            params[k] = str(v)
    res = simulation(f'{scheduler_args.trace_dir}/{task["trace_file"]}', task['cache_type'], task['cache_size'], params)
    elapsed = timeit.default_timer() - start_time
    print(f'time for trace_file {task["trace_file"]}, icache_type {task["cache_type"]}, '
          f'cache_size {task["cache_size"]}: {elapsed} second')
    # print(res)
    if scheduler_args.write_dir is not None:
        print('writing result back...')
        if not pathlib.Path(scheduler_args.write_dir).exists():
            pathlib.Path(scheduler_args.write_dir).mkdir(parents=True)
        timestamp = arrow.utcnow().float_timestamp
        with open(f'{scheduler_args.write_dir}/{timestamp}.res', 'w') as f:
            res = {
                'byte_hit_rate': float(res['byte_hit_rate']),
                'object_hit_rate': float(res['object_hit_rate']),
                'segment_byte_hit_rate': [float(r) for r in res['segment_byte_hit_rate'].split()],
                'segment_object_hit_rate': [float(r) for r in res['segment_object_hit_rate'].split()],
                'simulation_time': elapsed,
            }
            yaml.dump({'res': res,
                       'scheduler_args': vars(scheduler_args),
                       'task': task}, f)
    else:
        print(res)
    return res


def runner_run(scheduler_args, tasks):
    # seq the task
    for i, task in enumerate(tasks):
        tasks[i] = (scheduler_args, {'task_id': i, **task})
    # debug mode, disable parallel
    if scheduler_args.debug:
        return [run_task(tasks[0])]
    else:
        # normal mode
        # use less CPU, otherwise some task may stuck in the middle,  causing the job not return
        with multiprocessing.Pool(multiprocessing.cpu_count()//2) as pool:
            for _ in tqdm.tqdm(pool.imap_unordered(run_task, tasks), total=len(tasks)):
                pass


def main():
    scheduler_args, tasks = parser.parse()
    return runner_run(scheduler_args, tasks)


if __name__ == '__main__':
    main()
