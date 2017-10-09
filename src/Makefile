main:
	yacc -d parser.y
	flex parser.l
	gcc -o ../bin/servidor main.c lists.c y.tab.c lex.yy.c -ly -lfl

debug:
	yacc -d parser.y
	flex parser.l
	gcc -g -o ../bin/servidor main.c lists.c y.tab.c lex.yy.c -ly -lfl

clean:
	rm lex.yy.c y.tab.c y.tab.h ../bin/servidor 
