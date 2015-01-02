import requests

def test_get(server):
    r = requests.get('http://' + server)
    assert r.status_code == 200
    assert r.content == b'hello world'

def test_head(server):
    r = requests.head('http://' + server)
    assert r.status_code == 200
    assert r.content == b''

def test_post(server):
    r = requests.post('http://' + server, data={'test': 'data'})
    assert r.status_code == 501
    assert r.content == b''

