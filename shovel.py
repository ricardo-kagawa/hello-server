from pathlib import Path
from subprocess import check_call, CalledProcessError
from shovel import task

SRC = 'errors.c', 'util.c', 'parser.c', 'server.c'
EXE = 'cserver'

@task
def compile(debug=False):
    try:
        cmd = ['gcc', '-o', str(Path(EXE))]
        if debug:
            cmd += ['-D', 'DEBUG']
        cmd += [str(Path(src)) for src in SRC]
        cmd += ['-lev']
        check_call(cmd)
    except CalledProcessError as e:
        print(e)

@task
def clean():
    Path(EXE).unlink()

