#!/usr/bin/env python3
"""CM03/04 native Attribute analysis radar: install / restore.
Does not require a particular executable checksum. Verifies the relevant PE
layout and hook bytes before patching. Incompatible executables are refused.
No modified game executable is distributed.
"""
import argparse
import os
from pathlib import Path
import shutil
import struct
import sys

ROOT = Path(__file__).resolve().parent
BASE = 0x400000
HOOK = 0x00986c45
CAPTURE_SITE = 0x004c24ea
PING_SITES = (0x004c2716, 0x004c3ed4)
PING_TARGET_ORIGINAL = 0x00706760
RADAR = 0x00f9e000
CAPTURE = 0x00f9e022
PING = 0x00f9e013
OLD_HOOK = bytes.fromhex('ff155c50ab00')
OLD_CAPTURE = bytes.fromhex('8d8f00070000')

def call(src,dst): return b'\xe8'+struct.pack('<i',dst-src-5)
def align(x, a): return (x+a-1)//a*a

def pe_info(data):
    if data[:2] != b'MZ': raise ValueError('Not an MZ executable')
    pe=struct.unpack_from('<I',data,0x3c)[0]
    if data[pe:pe+4]!=b'PE\0\0': raise ValueError('Not a PE executable')
    fh=pe+4; oh=pe+24
    if struct.unpack_from('<H',data,fh)[0]!=0x14c or struct.unpack_from('<H',data,oh)[0]!=0x10b:
        raise ValueError('Only Win32 x86 PE is supported')
    if struct.unpack_from('<I',data,oh+28)[0]!=BASE: raise ValueError('Image base differs')
    sh=oh+struct.unpack_from('<H',data,fh+16)[0]
    n=struct.unpack_from('<H',data,fh+2)[0]
    sec=[]
    for j in range(n):
        pos=sh+40*j
        name,vs,rva,rs,raw=struct.unpack_from('<8sIIII',data,pos)
        sec.append((name.rstrip(b'\0'),vs,rva,rs,raw,pos))
    def fileoff(va):
        rva=va-BASE
        for _,vs,srva,rs,raw,_ in sec:
            if srva<=rva<srva+rs: return raw+rva-srva
        raise ValueError('No file-backed data at '+hex(va))
    return fh,oh,sh,sec,fileoff

def validate_patched(data,payload):
    fh,oh,sh,sec,off=pe_info(data)
    if [s[0] for s in sec] != [b'.text',b'.rdata',b'.data',b'.rsrc',b'.tjmp']:
        return False
    name,vs,rva,rs,raw,pos=sec[-1]
    if rva != RADAR-BASE or rs!=4096 or vs!=len(payload):return False
    if struct.unpack_from('<I',data,pos+36)[0]!=0xe0000020:return False
    if data[raw:raw+rs]!=payload.ljust(rs,b'\0'):return False
    if data[off(HOOK):off(HOOK)+6] != call(HOOK,RADAR)+b'\x90': return False
    if data[off(CAPTURE_SITE):off(CAPTURE_SITE)+6]!=call(CAPTURE_SITE,CAPTURE)+b'\x90':return False
    for s in PING_SITES:
        if data[off(s):off(s)+5]!=call(s,PING):return False
    return True

def install_pristine(original,payload):
    data=bytearray(original)
    fh,oh,sh,sec,off=pe_info(data)
    if [s[0] for s in sec] != [b'.text',b'.rdata',b'.data',b'.rsrc']:
        raise ValueError('Unexpected original PE section layout')
    if (struct.unpack_from('<II',data,oh+32)) != (0x1000,0x1000):
        raise ValueError('Unexpected PE file/section alignment')
    if sh+40*5 > min(s[4] for s in sec):
        raise ValueError('No room for an extra PE section header')
    if data[off(HOOK):off(HOOK)+6] != OLD_HOOK or data[off(CAPTURE_SITE):off(CAPTURE_SITE)+6]!=OLD_CAPTURE:
        raise ValueError('Unknown hook bytes in original executable')
    for s in PING_SITES:
        if data[off(s):off(s)+5]!=call(s,PING_TARGET_ORIGINAL):
            raise ValueError('Unknown profile callsite bytes')
    _,last_vs,last_rva,last_raw_size,_,_=sec[-1]
    new_rva=align(last_rva+max(last_vs,last_raw_size),4096)
    if new_rva!=RADAR-BASE:raise ValueError('Injected code virtual address would differ')
    raw=align(len(data),4096)
    data[off(HOOK):off(HOOK)+6]=call(HOOK,RADAR)+b'\x90'
    data[off(CAPTURE_SITE):off(CAPTURE_SITE)+6]=call(CAPTURE_SITE,CAPTURE)+b'\x90'
    for s in PING_SITES:
        data[off(s):off(s)+5]=call(s,PING)
    if len(data)<raw:data.extend(b'\0'*(raw-len(data)))
    data.extend(payload.ljust(4096,b'\0'))
    struct.pack_into('<8sIIIIIIHHI',data,sh+4*40,b'.tjmp\0\0\0',len(payload),new_rva,4096,raw,0,0,0,0,0xe0000020)
    struct.pack_into('<H',data,fh+2,5)
    struct.pack_into('<I',data,oh+56,align(new_rva+4096,4096))
    size_code=struct.unpack_from('<I',data,oh+4)[0]
    struct.pack_into('<I',data,oh+4,size_code+4096)
    struct.pack_into('<I',data,oh+64,0)
    patched=bytes(data)
    if not validate_patched(patched,payload):raise ValueError('Internal patch validation failed')
    return patched

def write_atomic(target,data):
    tmp=target.with_name(target.name+'.radar-temp')
    try:
        tmp.write_bytes(data)
        os.chmod(tmp,target.stat().st_mode)
        os.replace(tmp,target)
    finally:
        if tmp.exists():tmp.unlink()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe',type=Path,help='Full path to your own cm0304.exe')
    group=ap.add_mutually_exclusive_group()
    group.add_argument('--restore',action='store_true',help='Restore the executable from the installer backup')
    group.add_argument('--skin-only',action='store_true',help='Only install skin player profile XML')
    ap.add_argument('--no-skin',action='store_true',help='Do not install Ter skin profile XML')
    a=ap.parse_args()
    exe=a.exe.expanduser().resolve()
    backup=exe.with_name(exe.name+'.radar-tailjmp-backup')
    skin=exe.parent/'data/skins/ter/panels/player profile.xml'
    skin_backup=skin.with_name('player profile.xml.attribute-analysis-backup')
    try:
        if not exe.is_file():raise ValueError('Missing executable: '+str(exe))
        payload=(ROOT/'src/radar.bin').read_bytes()
        if len(payload)>4096:raise ValueError('Payload exceeds injected section')
        before=exe.read_bytes()
        if a.restore:
            if not backup.is_file():
                raise ValueError('Original executable backup missing')
            if not validate_patched(before,payload):
                raise ValueError('EXE does not match this patch; refusing to overwrite')
            original=backup.read_bytes()
            # A backup is not trusted merely because it has the expected filename.
            # Validate its code locations before allowing it to replace the game.
            install_pristine(original,payload)
            write_atomic(exe,original)
            if skin_backup.exists():
                shutil.copy2(skin_backup,skin)
                skin_backup.unlink()
            print('Restored original executable and previous skin XML (if backup exists).')
            return
        if not a.skin_only:
            if validate_patched(before,payload):
                print('Native payload already installed.')
            else:
                # No file-wide checksum requirement: validate the known hook
                # instructions and PE geometry before writing anything.
                patched=install_pristine(before,payload)
                if backup.exists():
                    if backup.read_bytes()!=before:
                        raise ValueError('An existing backup differs from this executable; refusing to overwrite it')
                else:
                    shutil.copy2(exe,backup)
                write_atomic(exe,patched)
                print('Patched compatible cm0304.exe (instruction bytes verified).')
        if not a.no_skin:
            src=ROOT/'skin/data/skins/ter/panels/player profile.xml'
            if not skin.parent.is_dir():
                raise ValueError('Ter skin panels folder not found: '+str(skin.parent))
            if skin.is_file() and not skin_backup.exists() and skin.read_bytes()!=src.read_bytes():
                shutil.copy2(skin,skin_backup)
            shutil.copy2(src,skin)
            print('Installed Ter player profile XML: Attribute analysis')
        print('Done. Close/restart the game to apply. Original backup:',backup)
    except (ValueError,OSError) as e:sys.exit('Stopped: '+str(e))
if __name__=='__main__':main()
