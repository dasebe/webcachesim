import argparse
import yaml

scheduler_args_default = {
    'debug': False
}


def parse_cmd_args():
    # how to schedule parallel simulations
    parser = argparse.ArgumentParser()
    parser.add_argument('--debug',
                        help='debug mode only run 1 task locally',
                        type=bool,
                        choices=[True, False])
    parser.add_argument('--config_file', type=str, nargs='?', help='runner configuration file')
    parser.add_argument('--algorithm_param_file', type=str, help='algorithm parameter config file')
    # parser.add_argument('--trace_file', type=str, nargs='+', help='path to trace file')
    # parser.add_argument('--cache_type', type=str, nargs='+', help='cache algorithms')
    # parser.add_argument('--cache_size', type=int, nargs='+', help='cache size in terms of byte')
    args = parser.parse_args()
    # worker_parser.add_argument('--n_warmup', type=int, help='number of requests to warmup the cache')
    # worker_parser.add_argument('--uni_size', type=int, help='whether assume each object has same size of 1')
    # read from PATH, WEBCACHESIM_TRACE_DIR
    # scheduler_parser.add_argument('--trace_dir',
    #                               type=str,
    #                               nargs='?',
    #                               help='whether the trace is placed')
    # scheduler_args, unknown_args = scheduler_parser.parse_known_args()

    # args that affects the result
    # worker_parser = argparse.ArgumentParser()
    # worker_args, unknown_args = worker_parser.parse_known_args(unknown_args)

    # args that don't affects the result
    # worker_extra_parser = argparse.ArgumentParser()
    # worker_extra_parser.add_argument('--segment_window', type=int, help='interval to record segment hit rate')
    # scheduler_parser.add_argument('--dburl', type=str)
    # worker_extra_args = worker_extra_parser.parse_args(unknown_args)

    # parser.add_argument('--n_early_stop',
    #                     type=int,
    #                     default=-1,
    #                     help='number of requests in total to exec; -1 means no early stop'
    #                     )
    # parser.add_argument('--tensorboard',
    #                     default=False,
    #                     action='store_true',
    #                     help='whether write tensorboard summary')

    return vars(args)


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


def job_to_tasks(args):
    """
    convert job config to list of task
    @:returns dict/[dict]
    """
    # job config file
    assert args.get('config_file') is not None
    with open(args['config_file']) as f:
        file_params = yaml.load(f)
    for k, v in file_params.items():
        if args.get(k) is None:
            args[k] = v

    # load algorithm parameters
    assert args.get('algorithm_param_file') is not None
    with open(args['algorithm_param_file']) as f:
        algorithm_params = yaml.load(f)
    for alg in algorithm_params:
        if alg in args:
            args[alg] = {**algorithm_params[alg], **args[alg]}
        else:
            args[alg] = algorithm_params[alg]
        args[alg] = extend_params(args[alg])

    tasks = []
    for cache_type in args['cache_type']:
        for cache_size in args['cache_size']:
            parameters_list = [{}] if args.get(cache_type) is None else \
                args[cache_type]
            for parameters in parameters_list:
                task = {
                    'trace_file': args['trace_file'],
                    'cache_type': cache_type,
                    'cache_size': cache_size,
                    **parameters,
                }
                for k, v in args.items():
                    if k not in ['cache_size', 'cache_type', 'trace_file'] and k not in algorithm_params and \
                            v is not None:
                        task[k] = v
                tasks.append(task)
    return tasks


def parse():
    """
    parse from cmd or kwargs.
    @:return: parsed nest dict
    """

    args = parse_cmd_args()
    tasks = job_to_tasks(args)
    if args["debug"]:
        print(tasks)
    return args, tasks
