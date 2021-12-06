	ORG 0H
	inc A
	AJMP HEY

	ORG 130H
HEY:	
	mov R2, #0FFH
	mov B, #0f0h
	add A, B
	sjmp $
	END



