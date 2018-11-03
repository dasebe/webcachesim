import multiprocessing
import tqdm
import arrow
import pathlib
from pywebcachesim.simulation import simulation
from pywebcachesim.runner import parser
import yaml


def run_task(args):
    scheduler_args, task = args
    res = simulation(task['trace_file'], task['cache_type'], task['cache_size'], {})
    # print(res)
    if scheduler_args.write_dir is not None:
        if not pathlib.Path(scheduler_args.write_dir).exists():
            pathlib.Path(scheduler_args.write_dir).mkdir(parents=True)
        timestamp = arrow.utcnow().float_timestamp
        with open(f'{scheduler_args.write_dir}/{timestamp}.res', 'w') as f:
            yaml.dump({'res': res, 'scheduler_args': vars(scheduler_args), 'task': task}, f)
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
