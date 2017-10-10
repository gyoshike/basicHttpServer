Servidor HTTP Básico escrito em C.
Dener Stassun Christinele
Guilherme Augusto Sakai Yoshike

Para rodar os testes com os resultados esperados, é necessário remover as permissões do diretório dir2 em /Webspace e remover as permissões dos arquivos dentro de Webspace/dir3/.
Dentro do diretorio testes, execute runTests.sh. Para verificar vazamentos de memorias, execute testMemLeaks.sh.
No momento, o teste que resulta no erro BAD REQUEST (400) resulta em segfault. Ainda é necessário lidar com o erro do parser para evitar este problema.
