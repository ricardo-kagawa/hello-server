import pytest

from pathlib import Path
from subprocess import check_call, Popen


@pytest.fixture(scope='session')
def server(request):
    src = Path('server.c')
    exe = Path('/tmp/cserver')
    check_call(('gcc', '-o', str(exe), str(src)))
    proc = Popen([str(exe)])

    def cleanup():
        try:
            proc.kill()
            proc.wait()
        finally:
            exe.unlink()
    request.addfinalizer(cleanup)

    return '127.0.0.1:8080'

