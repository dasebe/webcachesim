from pywebcachesim.runner import parser
import time
import subprocess


def to_task_str(task: dict):
    """
    split deterministic args and nodeterminstics args. Add _ prefix to later
    """

    params = {}
    for k, v in task.items():
        if k not in ['debug', 'config_file', 'algorithm_param_file', 'trace_file', 'cache_type', 'cache_size'] and v is not None:
            params[k] = str(v)
    params = [f'{k} {v}'for k, v in params.items()]
    params = ' '.join(params)
    res = f'webcachesim_cli_db {task["trace_file"]} {task["cache_type"]} {task["cache_size"]} {params}'
    return res


def runner_run(args: dict, tasks: list):
    # debug mode, only 1 task
    if args["debug"]:
        tasks = tasks[:1]

    ts = int(time.time())
    print(f'n_task: {len(tasks)}\n '
          f'generating job file to /tmp/{ts}.job')
    with open(f'/tmp/{ts}.job', 'w') as f:
        for i, task in enumerate(tasks):
            task_str = to_task_str(task)
            # f.write(task_str+f' &> /tmp/{ts}.log\n')
            ts_task = int(time.time()*1000000)
            task_str = f'bash --login -c "{task_str}" &> /tmp/{ts_task}.log\n'
            if i == 0:
                print(f'first task: {task_str}')
            f.write(task_str)
    p = subprocess.Popen(
        ['bash', '-ic', f'parallel -v --sshloginfile nodefile --sshdelay 1 < /tmp/{ts}.job'],
    )
    out, err = p.communicate()


def main():
    args, tasks = parser.parse()
    return runner_run(args, tasks)


if __name__ == '__main__':
    main()
