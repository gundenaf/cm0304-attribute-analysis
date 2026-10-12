.intel_syntax noprefix
.section .text.hook,"ax",@progbits
.global radar_hook
.global profile_ping
.global attributes_capture
.extern radar_draw
.extern g_profile_counter
.extern g_attrs
.extern g_have_attrs

radar_hook:
    pushfd
    pushad
    push esi
    call radar_draw
    add esp, 4
    popad
    popfd
    jmp DWORD PTR ds:0x00ab505c

profile_ping:
    mov DWORD PTR [g_profile_counter], 20
    jmp 0x00706760

/* Runs in place of LEA ECX,[EDI+0x700] at 004c24ea.
   Result ECX is preserved exactly, and all other registers/flags preserved. */
attributes_capture:
    lea ecx, [edi+0x700]
    pushfd
    pushad
    mov esi, ecx
    lea edi, [g_attrs]
    mov ecx, 42
    cld
    rep movsb
    mov DWORD PTR [g_have_attrs], 1
    popad
    popfd
    ret
