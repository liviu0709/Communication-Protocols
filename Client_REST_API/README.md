# Tema 4 PCOM

## Biblioteca parsare JSON
Am ales sa folosesc biblioteca nlohmann/json, pentru ca am implementat tema in C++, iar cealalta biblioteca era in C. C++ permite un nivel de abstractizare mai inalt, iar biblioteca nlohmann/json este foarte usor de folosit.

## Flow
Instantiem si pornim un client in main. La fiecare comanda citita de la stdin, tratam cazul cum se cuvine. Daca trebuie citite alte argumente, vor fi citite conform cerinta(punand un prompt, de ex username=) etc. Dupa primirea tuturor argumentemtelor necesare, clientul face un request la server, conform API si cerinta. Daca raspunsul contine datele necesare pentru afisare(sau pur si simplu da un cod de succes), se afiseaza succes(eventual si alte lucruri cerute precum lista de useri) si se merge mai departe(inapoi in run client), asteptand o alta comanda.

## Protocol HTTP
Cerinta nu a mentionat clar daca se doreste mentinerea unei conexiuni persistente sau nu. Astfel, am ales varianta mai usor de implementat, adica inchiderea conexiunii dupa fiecare reply primit.

## Validare input
Pentru a asigura o functionare robusta a clientului, validam inputul primit de la utilizator. Astfel, clientul notifica clientul daca a introdus tipul de date incorect si se repeta citirea pana cand utilizatorul ofera niste date corecte.

## Structura
Tema a fost implementata in C++, folosind conceptele de baza OOP. Astfel, exista o clasa Client care contine toate metodele necesare pentru a executa fiecare comanda primita de la tastatura. Clasele HTTP_Request si HTTP_Reply sunt folosite pentru a parsa datele primite/trimise catre server. Clasa Connection se ocupa cu trimiterea comunicarea efectiva dintre client si server, transmitand si primind datele necesare.

## Error 500
Serverul nu raspunde tot timpul corect la cereri. Din cand in cand, mai arunca si niste erori interne. Acestea sunt ignorate, trimitand requestul catre server pana cand nu mai primeste aceasta minunanta eroare.

## Get collection
Aceasta comanda pare sa fie singura care necesita multiple cereri catre server. Mai intai, se creeaza o colectie(iar serverul ne da id ul ei in reply), apoi se adauga un anumit numar de filme la ea, in functie de id. In final, pentru afisare conform cerinta(a titlului filmului care nu il cunoastem) se mai face inca un request catre server pentru colectia proaspat definita.

## Inconsistente checker
Afisarea detaliilor colectiei proaspat adaugate cu add_collection, conform cerinta, duce la putin undefined behaviour la rularea checkerului. Astfel, exista sansa sa dea o eroare la urmatoarea comanda(lista colectiilor), deoarece considera ca liniile cu filmele de la colectia proaspat adaugata fac parte din lista get collections. Afisand tot outputul lui add_collection intr-un singur cout, pare sa nu mai dea eroare. In plus, daca nu sunt afisate detaliile colectiei(la add collection), conform cerinta, checkerul chiar coloreaza mesajul de succes in verde. Consider ca nu asta se vrea de la noi, si afisez si detaliile colectiei create (chiar daca nu mai avem mesaj verde in checker).