"""Independent GCP1 decoder; never repairs CRC or guesses words."""
import json,struct,zlib
from pathlib import Path
FIELDS=json.loads((Path(__file__).with_name('record-fields.json')).read_text())
def decode(raw):
 if len(raw)!=320:raise ValueError('GCP1 size')
 w=struct.unpack('<80I',raw)
 if w[:3]!=(0x31504347,1,320):raise ValueError('GCP1 identity')
 if w[3]!=zlib.crc32(raw[16:]):raise ValueError('GCP1 CRC')
 if w[4]>14 or w[5]>14 or w[7]>127 or w[79]:raise ValueError('GCP1 fields')
 if w[4]==13 and (w[6] or w[7]!=127 or w[24]!=103 or w[25]!=103 or w[41]!=4096 or w[43]!=1):raise ValueError('GCP1 incomplete proof')
 out=dict(zip(FIELDS,w));out['crc32']=f'{w[3]:08x}'
 for n,i in [('epoch',54),('start',56),('end',58)]:out[n]=w[i]|w[i+1]<<32
 return out
