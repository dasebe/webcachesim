import sys
import argparse
import yaml
import numpy as np


def parse_cmd_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--trace_files', type=str, nargs='+', help='path to trace file')
    parser.add_argument('--cache_types', type=str, nargs='+', help='cache algorithms')
    parser.add_argument('--cache_sizes', type=int, nargs='+', help='cache size in terms of byte')
    # parser.add_argument('--config_file', type=str, nargs='?', help='runner configuration file')
    # parser.add_argument('--algorithm_param_file', type=str, help='algorithm parameter config file')
    parser.add_argument('--debug',
                        action='store_true',
                        help='debug mode only run 1 task locally'
                        )
    # parser.add_argument('--n_warmup',
    #                     type=int,
    #                     default=0,
    #                     help='number of requests to warmup the cache'
    #                     )
    # parser.add_argument('--n_early_stop',
    #                     type=int,
    #                     default=-1,
    #                     help='number of requests in total to exec; -1 means no early stop'
    #                     )
    # parser.add_argument('--if_dump_report',
    #                     default=False,
    #                     action='store_true',
    #                     help='whether dump the simulation result')
    # parser.add_argument('--if_uni_size',
    #                     default=False,
    #                     action='store_true',
    #                     help='whether assume each object has same size of 1')
    # parser.add_argument('--tensorboard',
    #                     default=False,
    #                     action='store_true',
    #                     help='whether write tensorboard summary')

    return parser.parse_args()


def job_to_tasks(args:argparse.Namespace):
    """
    convert job config to list of task
    """
    tasks = []
    for trace_file in args.trace_files:
        for cache_type in args.cache_types:
            for cache_size in args.cache_sizes:
                # parameters_list = [{}] if args.algorithm_parameters.get(algorithm) is None else \
                #     args.algorithm_parameters[algorithm]
                # for parameters in parameters_list:
                task = {
                    'trace_file': trace_file,
                    'trace_type': cache_type,
                    'cache_size': cache_size,
                    # 'n_warmup': args.n_warmup,
                    # 'n_early_stop': args.n_early_stop,
                    # 'if_uni_size': args.if_uni_size,
                    # **parameters,
                }
                tasks.append(task)
    return tasks


def parse(**kwargs):
    """
    parse from cmd or kwargs.
    @:return: parsed nest dict
    """

    args = parse_cmd_args()
    # with open(config.algorithm_parameters) as f:
    #     config.algorithm_parameters = yaml.load(f)

    # trace_parameters = {}
    # for trace in config['traces']:
    #     with open(f'{config.sushi_data_root}/datasets/{trace}_meta.yaml') as f:
    #         trace_parameters.update({trace: yaml.load(f)})
    # config.trace_parameters = trace_parameters

    tasks = job_to_tasks(args)
    # print(tasks)
    return tasks  #, config
