Servidor HTTP Básico escrito em C.
Dener Stassun Christinele
Guilherme Augusto Sakai Yoshike

Com o openssl instalado, é necessário criar um certificado assinado com a chave privada pelo servidor. Para isso, execute o seguinte código, que irá gerar a chave e o certificado
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365

Esse comando irá pedir alguns parâmetros. Os principais são o primeiro que é uma senha utilizada para a chave privada e o penúltimo que pede o nome do servidor. No caso, utilizaremos localhost.

Para compiliar o código, va até o diretório src e execute o make

Para rodar os testes com os resultados esperados, é necessário remover as permissões do diretório dir2 em /Webspace e remover as permissões dos arquivos dentro de Webspace/dir3/.
Dentro do diretorio testes, execute runTests.sh. Para verificar vazamentos de memorias, execute testMemLeaks.sh.
No momento, o teste que resulta no erro BAD REQUEST (400) resulta em segfault. Ainda é necessário lidar com o erro do parser para evitar este problema.

No diretório bin, execute o arquivo servidor
Os parâmetros são:
1 - pasta Webspace
2 - arquivo log
3 - porta para conexão do socket
4 - tempo de aguardo de timeout
5 - arquivo do certificado do servidor
6 - chave privada do servidor

Exemplo: ./servidor ../Webspace/ log.txt 9876 100 cert.pem key.pem
