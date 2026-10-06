import json, queue, subprocess, threading, time
class Client:
    def __init__(self,exe,*args,timeout=45):
        self.p=subprocess.Popen([str(exe),*args],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,encoding='utf-8',creationflags=subprocess.CREATE_NO_WINDOW)
        self.q=queue.Queue();self.seq=0;self.timeout=timeout
        def reader():
            for line in self.p.stdout:self.q.put(line)
        threading.Thread(target=reader,daemon=True).start()
    def rpc(self,method,params=None):
        self.seq+=1;req=dict(jsonrpc='2.0',id=self.seq,method=method)
        if params is not None:req['params']=params
        self.p.stdin.write(json.dumps(req,ensure_ascii=False)+'\n');self.p.stdin.flush()
        return json.loads(self.q.get(timeout=self.timeout))
    def raw(self,name,args=None):
        r=self.rpc('tools/call',dict(name=name,arguments=args or {}));assert 'result' in r,r
        return r['result']
    def call(self,name,args=None):
        r=self.raw(name,args)
        if r.get('isError'):raise AssertionError((name,r))
        text=r['content'][0]['text']
        try:return json.loads(text)
        except ValueError:return text
    def status(self):return self.call('editor_status')
    def edit(self,name,args=None):return self.call(name,dict(args or {},expected_revision=self.status()['revision']))
    def wait(self,value,limit=90):
        if not isinstance(value,dict) or 'job_id' not in value:return value
        end=time.monotonic()+limit
        while time.monotonic()<end:
            r=self.call('editor_job_status',{'job_id':value['job_id']})
            if r['state']!='running':
                if r['state']!='completed' or r['result'].get('isError'):raise AssertionError(r)
                text=r['result']['content'][0]['text']
                try:return json.loads(text)
                except ValueError:return text
            time.sleep(.08)
        raise AssertionError('Job timeout '+str(value))
