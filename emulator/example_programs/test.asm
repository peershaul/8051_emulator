	mov B, #0FFH
	mov A, #0
LOOP:
	inc A
	djnz B, LOOP
