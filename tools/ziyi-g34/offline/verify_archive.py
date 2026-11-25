"""Verify exact pinned Mesa file contents, types and executable modes.

Gitiles synthesizes tar/gzip metadata per request. Those byte checks failed in
the two retained initial attempts. This canonical content identity excludes
only container order/timestamps; every source file and symlink remains bound.
"""
from pathlib import Path,PurePosixPath
import hashlib,json,sys,tarfile

def identity(archive):
    rows=[];names=set()
    with tarfile.open(archive,'r:gz')as tar:
        for m in tar:
            p=PurePosixPath(m.name)
            assert not p.is_absolute() and '..'not in p.parts and m.name not in names
            names.add(m.name)
            if m.isdir():continue
            if m.isfile():
                h=hashlib.sha256(); f=tar.extractfile(m); assert f
                for block in iter(lambda:f.read(1048576),b''):h.update(block)
                rows.append([m.name,'file',m.mode&0o111,m.size,h.hexdigest()])
            else:
                assert m.issym() and not PurePosixPath(m.linkname).is_absolute()
                rows.append([m.name,'symlink',m.mode&0o111,m.linkname])
    rows.sort()
    digest=hashlib.sha256(json.dumps(rows,ensure_ascii=True,separators=(',',':')).encode()).hexdigest()
    return dict(files=len(rows),canonicalSha256=digest,commit='85fda3b36405bcaea8bffc64aa4f2651dc90770d')

if __name__=='__main__':
    result=identity(sys.argv[1])
    expected=json.loads(Path(sys.argv[2]).read_text())
    assert result==expected,(result,expected)
    Path(sys.argv[3]).write_text(json.dumps(dict(result='EXACT_MESA_CONTENT_PASS',**result),indent=2)+'\n')
    print('EXACT_MESA_CONTENT_PASS',result['files'],result['canonicalSha256'])
