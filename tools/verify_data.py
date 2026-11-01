#!/usr/bin/env python3
import hashlib,json,sys
from pathlib import Path
root=Path(sys.argv[1] if len(sys.argv)>1 else 'data/ARENA2')
man=json.load(open(Path(__file__).resolve().parents[1]/'data/MANIFEST.json'))
bad=0
for e in man['files']:
 f=root/e['file']
 if not f.exists():print('MISSING',e['file']);bad+=1;continue
 h=hashlib.sha256(f.read_bytes()).hexdigest()
 if h!=e['sha256']:print('CHANGED',e['file']);bad+=1
print('checked',len(man['files']),'bad',bad)
sys.exit(1 if bad else 0)
