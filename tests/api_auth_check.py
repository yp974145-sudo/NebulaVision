"""本机API集成检查；读取单独的本机账号文件，不打印密码和令牌。"""
import json,urllib.request,urllib.error,sys,time
from pathlib import Path
base='http://127.0.0.1:18880'
def call(path,body=None,token=''):
 headers={'Content-Type':'application/json'}
 if token:headers['Authorization']='Bearer '+token
 req=urllib.request.Request(base+path,data=None if body is None else json.dumps(body).encode(),headers=headers)
 try:
  with urllib.request.urlopen(req,timeout=10) as res:return res.status,json.load(res)
 except urllib.error.HTTPError as e:return e.code,json.load(e)
accounts=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'))
assert call('/api/events')[0]==401
assert call('/api/login',{'username':accounts['admin']['username'],'password':'wrong-test-value'})[0]==401
tokens={}
for role in ['admin','viewer']:
 code,data=call('/api/login',accounts[role]);assert code==200,(role,code);tokens[role]=data['token']
assert call('/api/events?size=0',token=tokens['admin'])[0]==400
assert call('/api/events?date=not-a-date',token=tokens['admin'])[0]==400
assert call('/api/events',token=tokens['admin'])[0]==200
assert call('/api/camera?id='+sys.argv[2],token=tokens['viewer'])[0]==403
assert call('/api/camera?id='+sys.argv[2],token=tokens['admin'])[0]==200
assert call('/api/logout',{},tokens['viewer'])[0]==200
assert call('/api/events',token=tokens['viewer'])[0]==401
if '--expiry' in sys.argv:
 time.sleep(3);assert call('/api/events',token=tokens['admin'])[0]==401
print('PASS: login, bad password, unauthenticated, forbidden resource, admin access, logout, parameter errors'+(', expiry' if '--expiry' in sys.argv else ''))
