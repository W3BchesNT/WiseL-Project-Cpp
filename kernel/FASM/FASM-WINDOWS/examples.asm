format PE64 GUI 4.0

include 'win64a.inc'

section '.text' code executable

  start:
    sub rsp,28h

    lea rcx,[wc]
    mov [rcx + wndclassex.cbSize],sizeof.wndclassex
    mov [rcx + wndclassex.style],CS_VREDRAW + CS_HREDRAW
    lea rax,[WndProc]
    mov [rcx + wndclassex.lpfnWndProc],rax
    lea rax,[app_name]
    mov [rcx + wndclassex.lpszClassName],rax
    mov [rcx + wndclassex.hbrBackground],COLOR_WINDOW+1

    call RegisterClassExW

    lea rcx,[app_name]
    lea rdx,[app_name]
    mov r8,WS_OVERLAPPEDWINDOW or WS_VISIBLE
    mov r9d,-2147483648
    call CreateWindowExW

    mov [hwnd],rax

    lea rcx,[msg]
.message_loop:
    call GetMessageW
    test eax,eax
    jz .exit
    lea rcx,[msg]
    call TranslateMessage
    lea rcx,[msg]
    call DispatchMessageW
    jmp .message_loop

.exit:
    xor ecx,ecx
    call ExitProcess

  WndProc:
    cmp edx,WM_DESTROY
    je .destroy
    cmp edx,WM_PAINT
    je .paint
    jmp DefWindowProcW

.destroy:
    xor ecx,ecx
    call PostQuitMessage
    xor eax,eax
    ret

.paint:
    sub rsp,40h
    lea rdx,[ps]
    call BeginPaint
    mov rcx,rax
    mov rdx,10
    mov r8d,10
    lea r9,[text]
    call TextOutW
    mov rcx,[hwnd]
    lea rdx,[ps]
    call EndPaint
    add rsp,40h
    xor eax,eax
    ret

section '.data' data

  app_name du "Viewer",0
  text du "Image Viewer",0

  wc wndclassex
  msg MSG
  ps PAINTSTRUCT

  hwnd dq 0
