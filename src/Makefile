main:
	yacc -d parser.y
	flex parser.l
	gcc -o servidor main.c lists.c y.tab.c lex.yy.c -ly -lfl

clean:
	rm lex.yy.c y.tab.c y.tab.h servidor output.txt log.txt
