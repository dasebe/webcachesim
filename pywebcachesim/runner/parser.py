import argparse
import yaml


scheduler_args_default = {
    'debug': False
}


def parse_cmd_args():
    # how to schedule parallel simulations
    scheduler_parser = argparse.ArgumentParser()
    scheduler_parser.add_argument('--debug',
                                  help='debug mode only run 1 task locally',
                                  type=bool,
                                  choices=[True, False])
    scheduler_parser.add_argument('--config_file', type=str, nargs='?', help='runner configuration file')
    scheduler_parser.add_argument('--trace_dir',
                                  type=str,
                                  nargs='?',
                                  help='whether the trace is placed')
    scheduler_parser.add_argument('--dburl', type=str)
    scheduler_parser.add_argument('--dbport', type=int)
    scheduler_parser.add_argument('--dbname', type=str)
    scheduler_parser.add_argument('--dbuser', type=str)
    scheduler_parser.add_argument('--dbpassword', type=str)
    scheduler_parser.add_argument('--dbcollection', type=str)

    scheduler_parser.add_argument('--algorithm_param_file', type=str, help='algorithm parameter config file')
    scheduler_args, unknown_args = scheduler_parser.parse_known_args()

    # args that affects the result
    worker_parser = argparse.ArgumentParser()
    worker_parser.add_argument('--trace_files', type=str, nargs='+', help='path to trace file')
    worker_parser.add_argument('--cache_types', type=str, nargs='+', help='cache algorithms')
    worker_parser.add_argument('--cache_sizes', type=int, nargs='+', help='cache size in terms of byte')
    worker_parser.add_argument('--n_warmup', type=int, help='number of requests to warmup the cache')
    worker_parser.add_argument('--uni_size', type=int, help='whether assume each object has same size of 1')
    worker_args, unknown_args = worker_parser.parse_known_args(unknown_args)

    # args that don't affects the result
    worker_extra_parser = argparse.ArgumentParser()
    worker_extra_parser.add_argument('--segment_window', type=int, help='interval to record segment hit rate')
    worker_extra_args = worker_extra_parser.parse_args(unknown_args)

    # parser.add_argument('--n_early_stop',
    #                     type=int,
    #                     default=-1,
    #                     help='number of requests in total to exec; -1 means no early stop'
    #                     )
    # parser.add_argument('--tensorboard',
    #                     default=False,
    #                     action='store_true',
    #                     help='whether write tensorboard summary')

    return vars(scheduler_args), vars(worker_args), vars(worker_extra_args)


def extend_params(param: dict):
    worklist = [param]
    res = []

    while len(worklist):
        p = worklist.pop()
        split = False
        for k in p:
            if type(p[k]) == list:
                _p = {_k: _v for _k, _v in p.items() if _k != k}
                for v in p[k]:
                    worklist.append({
                        **_p,
                        k: v,
                    })
                split = True
                break

        if not split:
            res.append(p)

    return res


def job_to_tasks(scheduler_args, worker_args, worker_extra_args):
    """
    convert job config to list of task
    @:returns dict/[dict]
    """
    # job config file
    if scheduler_args.get('config_file') is not None:
        with open(scheduler_args['config_file']) as f:
            file_params = yaml.load(f)
        if 'scheduler_args' in file_params:
            for k, v in file_params['scheduler_args'].items():
                av = scheduler_args.get(k)
                if av is None:
                    scheduler_args[k] = v

        if 'worker_args' in file_params:
            for k, v in file_params['worker_args'].items():
                av = worker_args.get(k)
                if av is None:
                    worker_args[k] = v

        if 'worker_extra_args' in file_params:
            for k, v in file_params['worker_extra_args'].items():
                av = worker_extra_args.get(k)
                if av is None:
                    worker_extra_args[k] = v

    # set default value
    for k, v in scheduler_args_default.items():
        if scheduler_args.get(k) is None:
            scheduler_args[k] = v

    # load algorithm parameters
    algorithm_params = {}
    if scheduler_args.get('algorithm_param_file') is not None:
        with open(scheduler_args['algorithm_param_file']) as f:
            algorithm_params = yaml.load(f)
        for alg in algorithm_params:
            algorithm_params[alg] = extend_params(algorithm_params[alg])

    tasks = []
    for trace_file in worker_args['trace_files']:
        for cache_type in worker_args['cache_types']:
            if worker_args.get("cache_sizes") is not None:
                cache_sizes = worker_args["cache_sizes"]
            else:
                cache_sizes = worker_args["trace_files"][trace_file]
            for cache_size in cache_sizes:
                parameters_list = [{}] if algorithm_params.get(cache_type) is None else \
                    algorithm_params[cache_type]
                for parameters in parameters_list:
                    task = {
                        'trace_file': trace_file,
                        'cache_type': cache_type,
                        'cache_size': cache_size,
                        **parameters,
                    }
                    for k, v in worker_args.items():
                        if k not in ['cache_sizes', 'cache_types', 'trace_files'] and v is not None:
                            task[k] = v
                    tasks.append(task)
    return scheduler_args, tasks, worker_extra_args


def parse():
    """
    parse from cmd or kwargs.
    @:return: parsed nest dict
    """

    scheduler_args, worker_args, worker_extra_args = parse_cmd_args()
    scheduler_args, tasks, worker_extra_args = job_to_tasks(scheduler_args, worker_args, worker_extra_args)
    if scheduler_args["debug"]:
        print(scheduler_args)
        print(tasks)
        print(worker_extra_args)
    return scheduler_args, tasks, worker_extra_args
