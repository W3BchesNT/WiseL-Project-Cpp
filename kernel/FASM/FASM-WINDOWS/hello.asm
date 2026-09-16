format PE64 CONSOLE
entry start

include 'C:\fasm\include\win64a.inc'

section '.data' data readable writeable
    hStdOut dq ?
    written dq ?
    str_1   db 'Hello World!', 13, 10
    str_len = $ - str_1

section '.text' code readable executable
start:
    sub     rsp, 40

    invoke  SetConsoleOutputCP, 65001
    invoke  GetStdHandle, STD_OUTPUT_HANDLE
    mov     [hStdOut], rax
    
    invoke  WriteFile, [hStdOut], str_1, str_len, written, 0
    invoke  ExitProcess, 0

section '.idata' import data readable writeable
    library kernel32, 'KERNEL32.DLL'
    import kernel32, \
        ExitProcess, 'ExitProcess', \
        GetStdHandle, 'GetStdHandle', \
        SetConsoleOutputCP, 'SetConsoleOutputCP', \
        WriteFile, 'WriteFile'
