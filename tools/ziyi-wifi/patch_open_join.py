"""Adapt exact pinned station source, preserving the existing WPA2 handshake."""
from pathlib import Path
import argparse, hashlib, json, shutil

def sha(b): return hashlib.sha256(b).hexdigest()

def adapt(original):
    s = original.decode(); changes = []
    def replace(old, new):
        nonlocal s
        assert s.count(old) == 1, (old[:100], s.count(old))
        s = s.replace(old, new, 1); changes.append(dict(before=sha(old.encode()), after=sha(new.encode())))
    replace('#include "lx_wpa.h"', '#include "lx_wpa.h"\n#include "ziyi_open_join.h"')
    replace('\tvolatile bool joined;\t\t\t/* keys set: frames pass */',
            '\tbool secured;\t\t\t\t/* WPA2 requires the real four-way handshake */\n\tvolatile bool joined;\t\t\t/* authorized: frames pass */')
    replace('\t\thandshake_message(radio, skb->data, skb->len);',
            '\t\tif (radio->secured) handshake_message(radio, skb->data, skb->len);')
    begin = s.index('\tradio->rsn[0] = 48;'); end = s.index('\tat += 22;', begin) + len('\tat += 22;')
    replace(s[begin:end], '\tat += zj_write_rsn(radio->rsn, at, group_cipher);')
    replace('\texplicit_bzero(&radio->wpa, sizeof(radio->wpa));\n\tfor (;',
            '\texplicit_bzero(&radio->wpa, sizeof(radio->wpa));\n\tradio->secured = false;\n\tfor (;')
    replace('\tint ret, error = 0, answer = ANSWER_NONE;',
            '\tint ret, error = 0, answer = ANSWER_NONE;\n\tbool secured = what->security == HAKIOS_STA_WPA2_PSK;')
    replace('if (what->security != HAKIOS_STA_WPA2_PSK)',
            'if (!secured && what->security != HAKIOS_STA_OPEN)')
    replace('lx: a network without WPA2 and a passphrase is asked for: that is not built',
            'lx: only explicitly requested open or WPA2-PSK infrastructure networks are supported')
    replace('\tINIT_WORK(&radio->group_work, group_rekey);',
            '\tradio->secured = secured;\n\tINIT_WORK(&radio->group_work, group_rekey);')
    replace('\tobstacle = protection(target, &group_cipher);',
            '\tobstacle = secured ? protection(target, &group_cipher) :\n\t    zj_open_bss(target->capability, target->elements, target->elements_length) ? NULL :\n\t    "open requested but the BSS is protected, malformed or not infrastructure";')
    replace('obstacle == NULL ? "WPA2 with a "\n\t    "passphrase and CCMP: it can be joined" : "IT CANNOT BE JOINED: ",',
            'obstacle == NULL ? (secured ? "WPA2-PSK/CCMP" : "open infrastructure") : "IT CANNOT BE JOINED: ",')
    # Zero state in both modes; execute no PMK/nonce/handshake work for open.
    begin = s.index('\tif (what->passphrase_length != 0) {', s.index('station_join(struct'))
    end = s.index('\tradio->handshake = LX_WPA_NOTHING;', begin)
    old = s[begin:end]
    replace(old, '\tbzero(radio->rsn, sizeof(radio->rsn));\n\tif (secured) {\n' +
            ''.join('\t'+line if line.strip() else line for line in old.splitlines(keepends=True)) + '\t}\n')
    replace('\t\tframe[0] = (uint8_t)(0x01 | 0x10 | (target->capability & 0x20));\n\t\tframe[1] = (uint8_t)((target->capability & 0x0500) >> 8);',
            '\t\tunsigned capability = zj_assoc_capability(target->capability, secured);\n\t\tframe[0] = (uint8_t)capability;\n\t\tframe[1] = (uint8_t)(capability >> 8);')
    begin = s.index('\t/* the handshake: the access point begins it;')
    end = s.index('\tif (ret != 0 || (ret = station_state(radio, IEEE80211_STA_ASSOC, IEEE80211_STA_AUTHORIZED)) != 0)', begin)
    old = s[begin:end]
    replace(old, '\tif (secured) {\n' + ''.join('\t'+line if line.strip() else line for line in old.splitlines(keepends=True)) +
            '\t\tif (ret != 0) { error = -ret; goto out; }\n\t} else {\n\t\tlx_tell("lx: OPEN association accepted; no RSN, handshake or keys required\\n");\n\t}\n')
    replace('if (ret != 0 || (ret = station_state(radio, IEEE80211_STA_ASSOC, IEEE80211_STA_AUTHORIZED)) != 0)',
            'if ((ret = station_state(radio, IEEE80211_STA_ASSOC, IEEE80211_STA_AUTHORIZED)) != 0)')
    return s.encode(), changes

def main():
    p=argparse.ArgumentParser(); p.add_argument('stage', type=Path); a=p.parse_args()
    source=Path(__file__).resolve().parent; stage=a.stage.resolve()
    original=source/'donor/wifi/lx/src/lx_80211.c'
    manifest=json.loads((source/'public-inputs.json').read_text())
    row=next(r for r in manifest['files'] if r['path']=='donor/wifi/lx/src/lx_80211.c')
    raw=original.read_bytes(); assert sha(raw)==row['sha256']
    target=stage/'wifi/lx/src/lx_80211.c'; assert target.read_bytes()==raw
    fixed, changes=adapt(raw)
    target.write_bytes(fixed)
    header=stage/'wifi/lx/src/ziyi_open_join.h'; assert not header.exists()
    shutil.copyfile(source/'open_join.h', header)
    receipt=dict(result='ZIYI_OPEN_JOIN_SOURCE_ADAPTED',sourceBeforeSHA256=sha(raw),sourceAfterSHA256=sha(fixed),
        headerSHA256=sha(header.read_bytes()),changes=changes,protectedRequestsNeverDowngraded=True,
        authorizationRequiresRealAuthAndAssoc=True,handshakeAndKeySetupPreservedForWPA2=True,
        nativeJoinProven=False,phoneOperated=False)
    with (stage/'open-join-adaptation.json').open('x') as f: json.dump(receipt,f,indent=2)
    print(json.dumps(dict(result=receipt['result'],sourceAfterSHA256=receipt['sourceAfterSHA256'],changes=len(changes))))
if __name__=='__main__': main()
