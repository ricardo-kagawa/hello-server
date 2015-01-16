from pathlib import Path
from subprocess import check_call
from shovel import task

SRC = Path('server.c')
EXE = Path('cserver')

@task
def compile(debug=False):
    cmd = ['gcc', '-o', str(EXE), str(SRC), '-lev']
    if debug:
        cmd += ['-D', 'DEBUG']
    check_call(cmd)

@task
def clean():
    EXE.unlink()

