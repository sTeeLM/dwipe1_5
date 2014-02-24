;
; hello.asm - a simple MBR routine that prints a message
; by Mark Loiseau
; http://blog.markloiseau.com/2012/05/hello-world-mbr-bootloader/
;

org 7c00h				; Tell NASM that the code's base will be at 7c00h.
						; Otherwise it will assume offset 0 when calculating
						; addresses.

jmp short start			; jump over data

message:
	db "disk has been wipped with dwipe<steel.mental@gmail.com>", 0dh, 0ah	
	db "hit any key to reboot", 0 ; null-terminated message

start:
	xor ax, ax			; clear ax
	mov ds, ax			; ds needs to be 0 for lodsb
	cld					; clear direction flag for lodsb

main:
	mov si, message		; move the message's address into si for lodsb
	jmp string			; jump to the string routine

; Displays a character
; int 0x10 ah=e
; al = character, bh = page number
char:
	mov bx, 0x1
	mov ah, 0xe
	int 0x10

; print a string
string:
	lodsb				; lodsb loads ds:esi into al
	cmp al, 0x0
	jnz char			; display character if not null

reboot:
	mov ax, 0
	int 0x16		; Wait for keystroke
	mov al,0FEh 
	out 64h,al 

times 0200h - 2 - ($ - $$)  db 0    ; NASM directive: zerofill up to 510 bytes

dw 0AA55h	; Magic boot sector signature
