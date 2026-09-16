format PE GUI 4.0
entry start
include 'win32a.inc'

; ==============================================================================
;                                   ÄÀÍÍÛÅ
; ==============================================================================
section '.data' data readable writeable
  _my_class    db 'MY_CLASS',0
  _vs_b_class  db 'W10_BUTTON',0
  _title       db 'Windows 10 Fluent UI',0
  _btn         db 'Confirm',0
  _font_name   db 'Segoe UI',0

  wc:
    .cbSize        dd 48
    .style         dd 3
    .lpfnWndProc   dd WindowProc
    .cbClsExtra    dd 0
    .cbWndExtra    dd 0
    .hInstance     dd ?
    .hIcon         dd 0
    .hCursor       dd 0
    .hbrBackground dd 0
    .lpszMenuName  dd 0
    .lpszClassName dd _my_class
    .hIconSm       dd 0

  wc_btn:
    .cbSize        dd 48
    .style         dd 3
    .lpfnWndProc   dd ButtonProc
    .cbClsExtra    dd 0
    .cbWndExtra    dd 0
    .hInstance     dd ?
    .hIcon         dd 0
    .hCursor       dd 0
    .hbrBackground dd 0
    .lpszMenuName  dd 0
    .lpszClassName dd _vs_b_class
    .hIconSm       dd 0

  hwnd_main      dd ?
  hwnd_btn       dd ?
  hfont          dd ?
  h_hand_cursor  dd ? ; Îðèãèíàëüíîå èìÿ ïåðåìåííîé

  w10_bg_brush     dd ? 
  w10_ctrl_brush   dd ? 
  w10_hover_brush  dd ? 
  w10_press_brush  dd ? 
  w10_border_pen   dd ? 
  
  btn_state      dd 0
  rect_struct    dd 4 dup(0)
  msg            MSG

; ==============================================================================
;                                   ÊÎÄ
; ==============================================================================
section '.code' code readable executable
start:
        invoke  GetModuleHandle, 0
        mov     [wc.hInstance], eax
        mov     [wc_btn.hInstance], eax
        
        invoke  RegisterClassExA, wc
        invoke  RegisterClassExA, wc_btn

        invoke  LoadCursorA, 0, 32649 ; IDC_HAND = 32649
        mov     [h_hand_cursor], eax

        invoke  CreateSolidBrush, 0x202020
        mov     [w10_bg_brush], eax
        invoke  CreateSolidBrush, 0x2D2D2D
        mov     [w10_ctrl_brush], eax
        invoke  CreateSolidBrush, 0x353535
        mov     [w10_hover_brush], eax
        invoke  CreateSolidBrush, 0x1F1F1F
        mov     [w10_press_brush], eax
        invoke  CreatePen, 0, 1, 0x3F3F46
        mov     [w10_border_pen], eax

        invoke  CreateWindowExA, 0, _my_class, _title, 0x00CF0000, 100, 100, 360, 150, 0, 0, 0, 0
        mov     [hwnd_main], eax
        
        invoke  GetWindowLongA, [hwnd_main], -16
        and     eax, not (0x00010000 or 0x00020000)
        invoke  SetWindowLongA, [hwnd_main], -16, eax
        
        invoke  GetSystemMenu, [hwnd_main], 0
        invoke  EnableMenuItem, eax, 0xF060, 0
        invoke  SetWindowPos, [hwnd_main], 0, 0, 0, 0, 0, 0x0027

        invoke  CreateFontA, 14, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, _font_name
        mov     [hfont], eax

        invoke  CreateWindowExA, 0, _vs_b_class, _btn, 0x50000000, 110, 40, 140, 32, [hwnd_main], 102, 0, 0
        mov     [hwnd_btn], eax

        invoke  ShowWindow, [hwnd_main], 1

msg_loop:
        invoke  GetMessageA, msg, 0, 0, 0
        or      eax, eax
        jz      end_loop
        invoke  TranslateMessage, msg
        invoke  DispatchMessageA, msg
        jmp     msg_loop
end_loop:
        invoke  ExitProcess, [msg.wParam]

; --- ÏÐÎÖÅÄÓÐÀ ÃËÀÂÍÎÃÎ ÎÊÍÀ ---
proc WindowProc hwnd,wmsg,wparam,lparam
        push    ebx esi edi
        cmp     [wmsg], 0x0014    ; WM_ERASEBKGND
        je      .w10_erase
        cmp     [wmsg], 0x0002    ; WM_DESTROY
        je      .wmdestroy

        pop     edi esi ebx
        invoke  DefWindowProcA, [hwnd], [wmsg], [wparam], [lparam]
        ret
.w10_erase:
        invoke  GetClientRect, [hwnd], rect_struct
        invoke  FillRect, [wparam], rect_struct, [w10_bg_brush]
        mov     eax, 1
        pop     edi esi ebx
        ret
.wmdestroy:
        invoke  DeleteObject, [hfont]
        invoke  DeleteObject, [w10_bg_brush]
        invoke  DeleteObject, [w10_ctrl_brush]
        invoke  DeleteObject, [w10_hover_brush]
        invoke  DeleteObject, [w10_press_brush]
        invoke  DeleteObject, [w10_border_pen]
        invoke  PostQuitMessage, 0
        xor     eax, eax
        pop     edi esi ebx
        ret
endp

; --- ÏÐÎÖÅÄÓÐÀ ÊÍÎÏÊÈ ---
proc ButtonProc hwnd,wmsg,wparam,lparam
        push    ebx esi edi

        cmp     [wmsg], 0x000F    ; WM_PAINT
        je      .btn_paint
        cmp     [wmsg], 0x0200    ; WM_MOUSEMOVE
        je      .btn_mousemove
        cmp     [wmsg], 0x02A3    ; WM_MOUSELEAVE
        je      .btn_mouseleave
        cmp     [wmsg], 0x0201    ; WM_LBUTTONDOWN
        je      .btn_lbuttondown
        cmp     [wmsg], 0x0202    ; WM_LBUTTONUP
        je      .btn_lbuttonup
        cmp     [wmsg], 0x0020    ; WM_SETCURSOR
        je      .btn_setcursor

        pop     edi esi ebx
        invoke  DefWindowProcA, [hwnd], [wmsg], [wparam], [lparam]
        ret

.btn_paint:
        sub     esp, 64
        mov     ebx, esp
        invoke  BeginPaint, [hwnd], ebx
        mov     esi, eax
        
        invoke  GetClientRect, [hwnd], rect_struct
        
        cmp     [btn_state], 2
        je      .paint_press
        cmp     [btn_state], 1
        je      .paint_hover
        
        invoke  FillRect, esi, rect_struct, [w10_ctrl_brush]
        jmp     .draw_border
.paint_hover:
        invoke  FillRect, esi, rect_struct, [w10_hover_brush]
        jmp     .draw_border
.paint_press:
        invoke  FillRect, esi, rect_struct, [w10_press_brush]

.draw_border:
        invoke  SelectObject, esi, [w10_border_pen]
        invoke  SelectObject, esi, [w10_ctrl_brush]
        invoke  Rectangle, esi, [rect_struct], [rect_struct+4], [rect_struct+8], [rect_struct+12]

        invoke  SelectObject, esi, [hfont]
        invoke  SetTextColor, esi, 0xFFFFFF
        invoke  SetBkMode, esi, 1
        invoke  DrawTextA, esi, _btn, -1, rect_struct, 0x00000025
        
        mov     ebx, esp
        invoke  EndPaint, [hwnd], ebx
        add     esp, 64
        xor     eax, eax
        pop     edi esi ebx
        ret

.btn_mousemove:
        cmp     [btn_state], 0
        jne     .skip_track
        mov     [btn_state], 1
        
        sub     esp, 16
        mov     dword [esp], 16
        mov     dword [esp+4], 2
        mov     eax, [hwnd]
        mov     [esp+8], eax
        mov     dword [esp+12], 0
        invoke  TrackMouseEvent, esp
        add     esp, 16
        
        push    1
        push    0
        push    [hwnd]
        call    [InvalidateRect]
.skip_track:
        xor     eax, eax
        pop     edi esi ebx
        ret

.btn_mouseleave:
        mov     [btn_state], 0
        push    1
        push    0
        push    [hwnd]
        call    [InvalidateRect]
        xor     eax, eax
        pop     edi esi ebx
        ret

.btn_lbuttondown:
        invoke  SetCapture, [hwnd]
        mov     [btn_state], 2
        push    1
        push    0
        push    [hwnd]
        call    [InvalidateRect]
        xor     eax, eax
        pop     edi esi ebx
        ret

.btn_lbuttonup:
        invoke  ReleaseCapture
        cmp     [btn_state], 2
        jne     .no_action
        mov     [btn_state], 1
        push    1
        push    0
        push    [hwnd]
        call    [InvalidateRect]
        invoke  SendMessageA, [hwnd_main], 0x0111, 102, [hwnd]
.no_action:
        xor     eax, eax
        pop     edi esi ebx
        ret

.btn_setcursor:
        ; Ïîôèêñåíî: Èñïîëüçóåì ïðàâèëüíîå èìÿ h_hand_cursor ñ íèæíèì ïîä÷åðêèâàíèåì
        invoke  SetCursor, [h_hand_cursor]
        mov     eax, 1
        pop     edi esi ebx
        ret
endp

; ==============================================================================
;                                  ÈÌÏÎÐÒÛ
; ==============================================================================
section '.idata' import data readable
  library kernel32,'KERNEL32.DLL',user32,'USER32.DLL',gdi32,'GDI32.DLL'
  import kernel32,GetModuleHandle,'GetModuleHandleA',ExitProcess,'ExitProcess'
  import user32,RegisterClassExA,'RegisterClassExA',CreateWindowExA,'CreateWindowExA',\
                GetWindowLongA,'GetWindowLongA',SetWindowLongA,'SetWindowLongA',GetSystemMenu,'GetSystemMenu',\
                EnableMenuItem,'EnableMenuItem',ShowWindow,'ShowWindow',SetWindowPos,'SetWindowPos',\
                GetMessageA,'GetMessageA',TranslateMessage,'TranslateMessage',DispatchMessageA,'DispatchMessageA',\
                DefWindowProcA,'DefWindowProcA',SendMessageA,'SendMessageA',FillRect,'FillRect',\
                GetClientRect,'GetClientRect',PostQuitMessage,'PostQuitMessage',SetFocus,'SetFocus',\
                BeginPaint,'BeginPaint',EndPaint,'EndPaint',TrackMouseEvent,'TrackMouseEvent',DrawTextA,'DrawTextA',\
                InvalidateRect,'InvalidateRect',LoadCursorA,'LoadCursorA',SetCursor,'SetCursor',\
                SetCapture,'SetCapture',ReleaseCapture,'ReleaseCapture'
  import gdi32,CreateFontA,'CreateFontA',CreateSolidBrush,'CreateSolidBrush',\
               SetTextColor,'SetTextColor',SetBkColor,'SetBkColor',DeleteObject,'DeleteObject',\
               SelectObject,'SelectObject',SetBkMode,'SetBkMode',CreatePen,'CreatePen',Rectangle,'Rectangle'
