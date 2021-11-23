	mov A, #5
	mov A, r4
	mov A, 20H
	mov R2, A
	mov R4, #110111b
	mov R5, 30
	mov @R1, A
	mov 1100b, @R0
	mov @R0, 30
	mov A, @R1	
	mov 20H, R2
	mov 40H, #20
	cpl a
