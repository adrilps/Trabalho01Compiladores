bison-calc: bison-calc.l bison-calc.y bison-calc.h
		bison -d bison-calc.y
		flex -o bison-calc.lex.c bison-calc.l
		gcc -o $@ bison-calc.tab.c bison-calc.lex.c bison-calc-func.c -lm
		@echo Parser da Calculadora com Cmds, funcoes, ... estah pronto!