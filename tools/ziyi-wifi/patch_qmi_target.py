"""Bind firmware lookup to the successful, decoded ath11k target response.

The original Linux release archive stays checksum-pinned by mk-ath11k.sh.
This single reviewed addition runs before calibration/BDF requests and never
calls the callback on a failed QMI response. No donor board is substituted.
"""
from pathlib import Path
import argparse,hashlib,json
SHA=''
def main():
 ap=argparse.ArgumentParser();ap.add_argument('stage',type=Path);a=ap.parse_args();out=a.stage.resolve()
 path=out/'xnu-deps/build/ath11k/src/ath11k/qmi.c';original=path.read_bytes()
 # Pinned copy recorded alongside this public transformation.
 expected=(out/'qmi-source-sha256.txt').read_text().strip();assert hashlib.sha256(original).hexdigest()==expected
 text=original.decode();anchor='\tif (resp.soc_info_valid)\n\t\tab->qmi.target.soc_id = resp.soc_info.soc_id;'
 assert text.count(anchor)==1
 new='''\t/* Ziyi: the platform selects its own ELF BDF only after a successful
\t * capabilities reply. An absent local file stops calibration. */
\textern int lx_os_qmi_target(unsigned int board_id);
\tret = lx_os_qmi_target(ab->qmi.target.board_id);
\tif (ret) goto out;

'''+anchor
 derived=text.replace(anchor,new).encode();backup=path.with_suffix('.c.ziyi-original');backup.open('xb').write(original)
 path.write_bytes(derived)
 (out/'qmi-target-adaptation.json').write_text(json.dumps(dict(originalSHA256=expected,derivedSHA256=hashlib.sha256(derived).hexdigest(),
  changes='One checked QMI target-to-native BDF callback, before any BDF/calibration; ELF selection matches own Xiaomi source',
  nativeExecuted=False),indent=2)+'\n')
 print('ZIYI_QMI_TARGET_HOOK_PREPARED')
if __name__=='__main__':main()
