from pathlib import Path
from subprocess import check_call
from shovel import task

SRC = Path('server.c')
EXE = Path('cserver')

@task
def compile():
    check_call(('gcc', '-o', str(EXE), str(SRC)))

@task
def clean():
    EXE.unlink()

