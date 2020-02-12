syntax keyword spyreInstruction ipush iadd isub imul idiv imod shl shr and or xor
syntax keyword spyreInstruction not neg igt ige ilt ile icmp jnz jz jmp call
syntax keyword spyreInstruction iret ccall fpush fadd fsub fmul fdiv fgt fge flt
syntax keyword spyreInstruction fle fcmp fret ilload ilsave iarg iload isave res ilea ider icinc cder lor land padd psub
syntax match spyreNumber "\v<\d+>"
syntax match spyreNumber "\v<\d+\.\d+>"
syntax match spyreNumber "\v<\d*\.?\d+([Ee]-?)?\d+>"
syntax match spyreNumber "\v<0x\x+([Pp]-?)?\x+>"
syntax match spyreNumber "\v<0b[01]+>"
syntax match spyreNumber "\v<0o\o+>"
syntax match spyreString "\v\".*\""
syntax keyword spyreSpecial let
syntax match spyreSpecial "\v\w+:"
syntax match spyreOperator "\v\*"
syntax match spyreOperator "\v/"
syntax match spyreOperator "\v\+"
syntax match spyreOperator "\v-"
syntax match spyreOperator "\v\?"
syntax match spyreOperator "\v\*\="
syntax match spyreOperator "\v/\="
syntax match spyreOperator "\v\+\="
syntax match spyreOperator "\v-\="
syntax match spyreOperator "\v:"
syntax match spyreOperator "\v\="
syntax match spyreOperator "\v\,"
syntax match spyreComment "\v\;.*$"
syntax match spyreCharacter "\v\'.+\'"

highlight link spyreInstruction Keyword
highlight link spyreNumber Number
highlight link spyreString String
highlight link spyreSpecial Type
highlight link spyreComment Comment
highlight link spyreCharacter Character
