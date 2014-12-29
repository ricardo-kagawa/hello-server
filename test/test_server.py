import pytest
import requests

def test_server(server):
    pytest.skip()
    r = requests.get('http://' + server)
    assert r.status_code == 200
    assert r.content == b'Hello world!'

