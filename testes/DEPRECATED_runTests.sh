#!/bin/bash
#NÃO RODAR
#Atualmente o servidor obtém sua entrada a partir de sockets e não de arquivos
#É necessário elaborar novos testes possivelmente utilizando telnet ou postman

tests=(./requests/*)

for ((i=0; i<${#tests[@]}; i++)); do
    requestFile="${tests[$i]}"
    outFile="./output/"
    outFile+=$(echo "$requestFile" | cut -d/ -f3)
    outFile+=".out"
    ../bin/servidor ../Webspace "$requestFile" "$outFile" ./log.txt
done
