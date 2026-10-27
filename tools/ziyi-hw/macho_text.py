"""Read defined Mach-O code symbols by section, independent of nm type letters."""
import struct

def text_symbols(data):
    assert len(data)>=32 and data[:4]==bytes.fromhex('cffaedfe'),'Expected ARM64 Mach-O'
    assert struct.unpack_from('<I',data,4)[0]==0x0100000c,'Expected ARM64 CPU'
    ncmds,command_bytes=struct.unpack_from('<II',data,16)
    end=32+command_bytes;assert end<=len(data)
    at=32;section_index=0;text={};symtab=None
    for _ in range(ncmds):
        assert at+8<=end
        cmd,size=struct.unpack_from('<II',data,at)
        assert size>=8 and at+size<=end
        if cmd==0x19:
            assert size>=72
            va,vs,off,fs=struct.unpack_from('<QQQQ',data,at+24)
            initial,nsects=struct.unpack_from('<II',data,at+60)
            assert off<=len(data) and fs<=len(data)-off and 72+80*nsects<=size
            for i in range(nsects):
                section_index+=1;p=at+72+80*i
                name=data[p:p+16].split(b'\0',1)[0]
                address,length,offset=struct.unpack_from('<QQI',data,p+32)
                if name!=b'__text' or not initial&4 or not length:continue
                assert va<=address and length<=va+vs-address
                assert off<=offset and length<=off+fs-offset
                assert offset<=len(data) and length<=len(data)-offset
                text[section_index]=dict(address=address,bytes=length,offset=offset,
                    segment=data[p+16:p+32].split(b'\0',1)[0].decode('ascii'))
        elif cmd==2:
            assert size>=24 and symtab is None
            symtab=struct.unpack_from('<IIII',data,at+8)
        at+=size
    assert at==end and text and symtab is not None
    off,count,strings,strings_size=symtab
    assert off<=len(data) and count*16<=len(data)-off
    assert strings<=len(data) and strings_size<=len(data)-strings
    symbols={};addresses={index:set() for index in text}
    for i in range(count):
        name,kind,section,desc,value=struct.unpack_from('<IBBHQ',data,off+16*i)
        if kind&0xe0 or kind&0x0e!=0x0e or section not in text or not name:continue
        assert name<strings_size
        finish=data.find(b'\0',strings+name,strings+strings_size);assert finish>=0
        entry=text[section]
        if not entry['address']<=value<entry['address']+entry['bytes']:continue
        label=data[strings+name:finish].decode('utf-8')
        symbols.setdefault(label,[]).append(dict(address=value,section=section,external=bool(kind&1)))
        addresses[section].add(value)
    return dict(symbols=symbols,sections=text,addresses=addresses)

def function_bytes(data,parsed,name):
    entries=parsed['symbols'].get(name,[])
    assert entries,(name,'missing defined executable __text symbol')
    identities={(e['address'],e['section']) for e in entries};assert len(identities)==1,name
    address,index=identities.pop();section=parsed['sections'][index]
    following=[v for v in parsed['addresses'][index] if v>address]
    stop=min(following) if following else section['address']+section['bytes']
    length=stop-address;assert length>=32,(name,length,'empty/inlined hook')
    offset=section['offset']+address-section['address'];sample=data[offset:offset+min(length,128)]
    assert len(sample)>=32 and any(sample) and sample[:4]!=bytes.fromhex('c0035fd6'),name
    return dict(name=name,address=address,section=index,segment=section['segment'],
                bytesToNextTextSymbol=length,sample=sample)
