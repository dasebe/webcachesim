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
    parser.add_argument('--trace_param_file', type=str, help='trace parameter config file')
    args = parser.parse_args()

    return vars(args)


def cartesian_product(param: dict):
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
        default_algorithm_params = yaml.load(f)

    assert args.get('trace_param_file') is not None
    with open(args['trace_param_file']) as f:
        trace_params = yaml.load(f)

    tasks = []
    for trace_file in args['trace_files']:
        for cache_type in args['cache_types']:
            for cache_size_or_size_parameters in trace_params[trace_file]['cache_sizes']:
                # element can be k: v or k: list[v], which would be expanded with cartesian product
                # priority: default < per trace < per trace per algorithm < per trace per algorithm per cache size
                parameters = {}
                if cache_type in default_algorithm_params:
                    parameters = {**parameters, **default_algorithm_params[cache_type]}
                per_trace_params = {}
                for k, v in trace_params[trace_file].items():
                    if k not in ['cache_sizes'] and k not in default_algorithm_params and v is not None:
                        per_trace_params[k] = v
                parameters = {**parameters, **per_trace_params}
                if cache_type in trace_params[trace_file]:
                    # trace parameters overwrite default parameters
                    parameters = {**parameters, **trace_params[trace_file][cache_type]}
                if isinstance(cache_size_or_size_parameters, dict):
                    # only 1 key (single cache size) is allowed
                    assert (len(cache_size_or_size_parameters) == 1)
                    cache_size = list(cache_size_or_size_parameters.keys())[0]
                    if cache_type in cache_size_or_size_parameters[cache_size]:
                        # per cache size parameters overwrite other parameters
                        parameters = {**parameters, **cache_size_or_size_parameters[cache_size][cache_type]}
                else:
                    cache_size = cache_size_or_size_parameters
                parameters_list = cartesian_product(parameters)
                for parameters in parameters_list:
                    task = {
                        'trace_file': trace_file,
                        'cache_type': cache_type,
                        'cache_size': cache_size,
                        **parameters,
                    }
                    for k, v in args.items():
                        if k not in [
                            'cache_types',
                            'trace_files',
                            'algorithm_param_file',
                            'trace_param_file',
                            'config_file'
                        ] and v is not None:
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
