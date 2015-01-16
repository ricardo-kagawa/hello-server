import pytest

from pathlib import Path
from signal import SIGINT
from subprocess import check_call, Popen, TimeoutExpired


@pytest.fixture(scope='session')
def server(request):
    src = Path('server.c')
    exe = Path('/tmp/cserver')
    check_call(('gcc', '-o', str(exe), str(src), '-lev'))
    proc = Popen([str(exe)])

    def cleanup():
        try:
            proc.send_signal(SIGINT)
            proc.wait(5)
        except TimeoutExpired:
            proc.terminate()
            proc.wait()
        finally:
            exe.unlink()
    request.addfinalizer(cleanup)

    return '127.0.0.1:8080'

