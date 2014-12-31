import requests

def test_server(server):
    r = requests.get('http://' + server)
    assert r.status_code == 200
    assert r.content == b'Hello, World!'

