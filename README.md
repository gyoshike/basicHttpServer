Servidor HTTP Básico escrito em C.
Dener Stassun Christinele
Guilherme Augusto Sakai Yoshike

Para realizar a build, crie um diretório /bin na pasta raíz (basicHttpServer) e rode o make em /src.
Para obter os resultados esperados, é necessário remover as permissões do diretório dir2 em /Webspace e remover as permissões dos arquivos dentro de Webspace/dir3/.
Para garantir que o retorno de recursos ocorra corretamente, limpe a cache do seu navegador. Caso o recurso requisitado esteja na cache, o comportamento do servidor é imprevisível.
Assim, para rodar o servidor, utilize './servidor ../Webspace/ log.txt'.
Os testes automáticos realizados pelo arquivo runTests.sh não funcionam mais pois agora o servidor obtém as requests a partir de um socket e não de arquivos de entrada.
