import yaml
from pygit2 import Repository
import subprocess

config_file = '/home/zhenyus/webcachesim/config/job_dev.yaml'
webcachesim_root = '/home/zhenyus/webcachesim'

with open(config_file) as f:
    params = yaml.load(f)

current_branch = Repository(webcachesim_root).head.shorthand
for node in params['nodes']:
    # for each machine, fetch, checkout, merge, make, make install
    hostname = node[node.find('/') + 1:] if '/' in node else node
    command = ['ssh', '-t', hostname, f'cd ${{WEBCACHESIM_ROOT}}/build; '
                                      f'git fetch; git checkout {current_branch}; git pull; make -j8; sudo make install']
    subprocess.run(command)

command = ['/home/zhenyus/anaconda3/envs/webcachesim_env/bin/python',
           '/home/zhenyus/webcachesim/pywebcachesim/runner/runner.py',
           '--authentication_file', '/home/zhenyus/webcachesim/config/authentication.yaml',
           '--config_file', '/home/zhenyus/webcachesim/config/job_dev.yaml',
           '--algorithm_param_file', '/home/zhenyus/webcachesim/config/algorithm_params.yaml',
           '--trace_param_file', '/home/zhenyus/webcachesim/config/trace_params.yaml',
           ]

subprocess.run(command)
