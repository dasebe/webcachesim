import multiprocessing
from pywebcachesim import parser
import ujson as json
import arrow
import tqdm
# from pywebcachesim import simulation

config = {}


def run_task(args):
    pass
    # res = simulation(args['trace_file'], args['cache_type'], args['cache_size'], args)
    # if config['dump_report']:
    #     timestamp = arrow.utcnow().float_timestamp
    #     with open(f'{config["sushi_data_root"]}/reports/{timestamp}.json', 'w') as f:
    #         json.dump({**res, **args['task']}, f)
    # return res


def runner_run(tasks):
    global config
    # seq the task
    for i, task in enumerate(tasks):
        tasks[i] = {
            'task': task,
            'task_id': i,
        }
    # debug mode, disable parallel
    if config.debug:
        return [run_task(tasks[0])]
    else:
        # normal mode
        # use less CPU, otherwise some task may stuck in the middle,  causing the job not return
        with multiprocessing.Pool(multiprocessing.cpu_count()//2) as pool:
            for _ in tqdm.tqdm(pool.imap_unordered(run_task, tasks), total=len(tasks)):
                pass


def main():
    task = parser.parse()
    return runner_run(task)


if __name__ == '__main__':
    main()
