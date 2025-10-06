# Tema 2 PCOM

## Biblioteci
Conform laborator, am utilizat poll.h pentru multiplexarea IO si socket.h pentru lucrul cu socket-urile UDP si TCP. Conform cerinta, algoritmul lui Nagle a fost dezactivat pentru socket-urile TCP.

## Protocol nivel aplicatie
Protocolol folosit este foarte trivial, deoarece aplicatia nu are nevoie de nimic complex.
+ Serverul trimite pachete care au un camp de tip int ce reprezinta lungimea mesajului, iar clientul citeste acest camp si stie cat de mult sa citeasca.Dupa aceea, afiseaza mesajul pe ecran.
+ Clientul are un header cu un camp in plus(foarte complex stiu), tipul mesajului:
    + subscribe (sir de caractere ce reprezinta topicul)
    + unsubscribe (sir de caractere ce reprezinta topicul)
    + id (sir de caractere ce reprezinta id-ul clientului)

## Flow client
1. Clientul se conecteaza la server
2. Clientul trimite un mesaj de tip id, care contine id-ul clientului
3. Clientul asteapta input de la stdin/date de la server pentru a le afisa la stdout
4. Clientul trimite comenzile de subscribe/unsubscribe catre server, iar serverul trimite mesajele corespunzatoare cand este cazul

## Flow server
+ Serverul multiplexeaza un socket UDP, un socket TCP pentru initializarea conexiunii si stdin
+ Serverul accepta orice conexiune TCP si asteapta un mesaj de tip id de la client
+ Serverul adauga clientul in lista de clienti activi
+ Serverul trimite mesajele corespunzatoare clientului, in functie de topicul la care s-a abonat
+ daca un client se deconecteaza se salveaza starea lui, dar i se pastreaza datele
+ daca un alt client incearca sa foloseasca acelasi id cu unul deja conectat, serverul ii inchide conexiunea la primirea id-ului

## Topic matching
Pentru topicurile fara wildcard, avem un hash map care la cheie topicul si la valoare un vector de id-uri ale clientilor care sunt abonati. Pentru topicurile cu wildcard, fiecare client are un vector de topicuri de wildcarduri. Cand un mesaj ajunge la server, se incearca trimiterea lui catre toti clientii din hash map, iar apoi se verifica daca mai exista clienti care nu au primit mesajul. Se verifica fiecare wildcard in parte al clientilor ramasi. Daca unul se potriveste, se trimite mesajul clientului si se trece la urmatorul client.

## Functionalitati bonus
Clientul si serverul afiseaza un mesaj de eroare la stdout in cazul in care comanda introdusa la stdin este invalida. <br>
Makefile-ul contine si niste reguli pentru a genera un executabil cu teste unitare pentru functia de pattern matching. Testele unitare se gasesc in test.cpp (daca exista cazuri limita care nu se gasesc acolo si nu functioneaza -> ghinion).

## Nota
Se considera ca unsubscribe este exact opusul subscribe-ului, deci abonatul nu va putea sa se dezaboneze de la un singur topic daca s-a abonat la mai multe simultan. (daca da subscribe la x, singura comanda de unsubscribe utila pentru el este unsubscribe x -> exact ca in checker)

