
.DATA

.CODE

GetCurrentKpcr	PROC
	mov rax, qword ptr  gs: [18h] ;
	ret
GetCurrentKpcr	ENDP

END