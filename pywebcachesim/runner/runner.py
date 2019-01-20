from pywebcachesim.runner import parser
import time
import subprocess


def to_task_str(scheduler_args: dict, task: dict, worker_extra_args: dict):
    """
    split deterministic args and nodeterminstics args. Add _ prefix to later
    """

    params = {}
    for k, v in task.items():
        if k not in ['trace_file', 'cache_type', 'cache_size']:
            params[k] = str(v)
    for k, v in worker_extra_args.items():
        if v is not None:
            params[k] = str(v)
    for k, v in scheduler_args.items():
        if k not in ['debug', 'config_file', 'algorithm_param_file'] and v is not None:
            params[k] = str(v)
    params = [f'{k}={v}'for k, v in params.items()]
    params = ' '.join(params)
    res = f'webcachesim_cli_db {task["trace_file"]} {task["cache_type"]} {task["cache_size"]} {params}'
    return res


def runner_run(scheduler_args: dict, tasks: list, worker_extra_args: dict):
    # debug mode, only 1 task
    if scheduler_args["debug"]:
        tasks = tasks[:1]

    ts = int(time.time())
    print(f'generating job file to /tmp/{ts}.job')
    with open(f'/tmp/{ts}.job', 'w') as f:
        for task in tasks:
            task_str = to_task_str(scheduler_args, task, worker_extra_args)
            f.write(task_str+'\n')
    p = subprocess.Popen(
        [f'parallel < /tmp/{ts}.job'],
        stdout=subprocess.PIPE,
        stdin=subprocess.PIPE,
        shell=True,
    )
    out, err = p.communicate()


def main():
    scheduler_args, tasks, worker_extra_args = parser.parse()
    return runner_run(scheduler_args, tasks, worker_extra_args)


if __name__ == '__main__':
    main()
