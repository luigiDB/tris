#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/time.h>

struct utente {
	char username[25];			//username
	char usernameA[25];			//username avversario
	short int portaUDP;			//porta socket di comunicazione P2P
	int socket;					//descrittore socket TCP
	unsigned long indirizzoIP;	//IP
	int stato;					//stato di gioco:	0 -> LIBERO
								//					1 -> OCCUPATO
	/*gestione fasi*/
	int fase;					//fase di connessione
	int lung;					//appoggio per la lunghezza stringhe
	/**/
	struct utente *avversario;	//puntatore avversario
	struct utente *puntatore;	//puntatore lista
};

struct utente* nuovo=0;				//appoggio
struct utente* utenti_connessi=0;	//pila
struct utente* provvisorio=0;		//pila
int n_utenti_connessi=0;
struct utente *client=0;

struct sockaddr_in serveraddr;
struct sockaddr_in clientaddr;
char comando;
int lung;
int listener;
fd_set master;
fd_set read_fds;
int fdmax;
int ret;

///////////////////FUNZIONI DI CONTROLLO DELL' ERRORE
void errorecv(int i,int j){
	if(ret!=i) {	
		printf("Errore durante il comando recv() n%d\n",j);
		exit(1);
	}
}

void checksend(int i,int j){
	if(ret!=i){
		printf("Errore durante il comando send() n%d\n",j);
		exit(1);
	}
}	


///////////////////FUNZIONI PER LA GESTIONE DELLA LISTA
void rimozione_elemento_da_lista(struct utente* elem) {	//RIMUOVI DALLA LISTA UTENTI_CONNESSI
	struct utente* temp=utenti_connessi;
	if(n_utenti_connessi==0) {
		return;
	}
	if(temp==elem) {
		utenti_connessi=utenti_connessi->puntatore;
		free(temp);
		n_utenti_connessi--;
		return;
	}
	while(temp->puntatore!=elem&&temp->puntatore!=NULL)
		temp=temp->puntatore;
	if(temp->puntatore==NULL)
		return;
	n_utenti_connessi--;
	temp->puntatore=temp->puntatore->puntatore;
	return;
}

void rimuovi_da_provvisorio(struct utente* elem) {		//RIMUOVI DALLA LISTA PROVVISORIO
	struct utente* temp=provvisorio;
	if(temp==elem) {
		provvisorio=provvisorio->puntatore;
	return;
	}
	while(temp->puntatore!=elem&&temp->puntatore!=NULL)
		temp=temp->puntatore;
	if(temp->puntatore==NULL)
		return;
	temp->puntatore=temp->puntatore->puntatore;
	return;
}

struct utente* trova_da_socket(int sd) {				//RITORNA L'UTENTE CON SOCKET SD DALLA LISTA UTENTI_CONNESSI usa socket 
	struct utente* temp=utenti_connessi;
	while(temp!=NULL) {
		if(temp->socket==sd)
			return temp;
		temp=temp->puntatore;
	}
	return NULL;
}

struct utente* trova_provvisorio(int sd) {				//RITORNA L'UTENTE CON SOCKET SD DALLA LISTA PROVVISORIO usa socket
	struct utente* temp=provvisorio;
	while(temp!=NULL) {
		if(temp->socket==sd)
			return temp;
		temp=temp->puntatore;
	}
	return NULL;
}

struct utente* trova_da_username(char* name) {			//RITORNA L'UTENTE CON NOME NAME DALLA LISTA UTENTI_CONNESSI usa name
	struct utente* temp=utenti_connessi;
	while(temp!=NULL) {
		if(strcmp(temp->username,name)==0)
			return temp;
		temp=temp->puntatore;
	}
	return NULL;
}

int exist(char* name) {									//VERIFICA CHE ESISTE NELLA LISTA UTENTI_CONNESSI
	struct utente* temp=utenti_connessi;
	while(temp!=NULL) {
		if(strcmp(temp->username,name)==0) 
			return 1;
		temp=temp->puntatore;
	}
	return 0;
}


///////////////////FUNZIONI CHE IMPLEMENTANO I COMANDI PER IL SERVER
void who() {
	int lung;
	struct utente* temp=utenti_connessi;
	//dice al client quanti utenti sono connessi
	ret=send(client->socket,(void*)&n_utenti_connessi,sizeof(int),0);
	checksend(sizeof(int),1);
	while(temp) {
		lung=strlen(temp->username);
		//lunghezza
		ret=send(client->socket,(void*)&lung,sizeof(int),0);
		checksend(sizeof(int),2);
		//username
		ret=send(client->socket,(void*)temp->username,lung,0);
		checksend(lung,3);
		//stato
		ret=send(client->socket,(void*)&temp->stato,sizeof(int),0);
		checksend(sizeof(int),4);
		temp=temp->puntatore;
	}
}

void disconnect(char cmd) {
	printf("%s si e' disconnesso dalla partita \n",client->username);
	printf("%s e' libero\n", client->username);
	if(cmd!='k') {	
		printf("%s si e' disconnesso dalla partita \n",client->avversario->username);
		printf("%s e' libero\n", client->avversario->username);
		client->avversario->stato=0;
		client->avversario->avversario=NULL;
			
	}
	client->stato=0;
	client->avversario=NULL;
}

///////////////////CONNESSIONE CON L'AVVERSARIO 
void connect1() {	//PRELEVO LA LUNGHEZZA DEL NOME UTENTE AVVERSARIO
	ret=recv(client->socket,(void*)&client->lung,sizeof(int),0);
	errorecv(sizeof(int),1);
	client->fase=5;
}

void connect2() {	//PRELEVO IL NOME UTENTE AVVERSARIO E SETUP CONNESSIONE P2P PER ENTRAMBI I PEER
	char cmd;
	//lunghezza username al quale si vuole connettere
	client->fase=0;	
	ret=recv(client->socket,(void*)&client->usernameA,client->lung,0);
	errorecv(client->lung,2);
	client->usernameA[client->lung]='\0';
	//dice il client che sta risposndendo al comando !connect
	if(!exist(client->usernameA)) {
		printf("Utente %s non trovato\n",client->usernameA);		
		cmd='i';			//COMANDO i -> MOME UTENTE NON ESISTENTE
		ret=send(client->socket,(void *)&cmd,sizeof(char),0);
		checksend(sizeof(char),6);
		return;
	}
	////INVIO AL DESTINATARIO DELLA RICHIESTA INFORMAZIONI SUL CLIENT CHE VUOLE GIOCARE CON LUI
	client->avversario=trova_da_username(client->usernameA);
	if(client->avversario->stato==1) {	
		printf("Utente %s occupato\n",client->usernameA);		
		cmd='b';			//COMANDO b -> UTENTE OCCUPATO
		ret=send(client->socket,(void *)&cmd,sizeof(char),0);
		checksend(sizeof(char),7);
		return;
	}
	//comando scelto per inoltrare la richiesta al client
	client->stato=1;
	client->avversario->stato=1;	
	client->avversario->avversario=client;
	//lunghezza username
	lung=strlen(client->username);
	ret=send(client->avversario->socket,(void *)&lung,sizeof(int),0);
	checksend(sizeof(int),9);
	//username
	ret=send(client->avversario->socket,(void *)&client->username,lung,0);
	checksend(lung,10);
}
//___________________________________________________________________________

void connect3(char cmd) {	//AZIONI PER CONNESSIONE P2P ACCETTATA O RIFIUTATA
	//////////////////GESTIONE DELLA RISPOSTA DEL DESTINATARIO DELLA RICIHESTA
	
	short int porta;
	switch(cmd) { //switch che gestisce la scelta del client interrogato
		case 'a': { //accetta
			ret=send(client->avversario->socket,(void *)&cmd,sizeof(char),0);
			checksend(sizeof(char),11);
			//invio della porta di client
			porta=htons(client->portaUDP);
			ret=send(client->avversario->socket,(void *)&porta,sizeof(porta),0);
			checksend(sizeof(porta),12);
			ret=send(client->avversario->socket,(void *)&client->indirizzoIP,sizeof(client->indirizzoIP),0);
			checksend(sizeof(client->indirizzoIP),13);
			//riempimento dei campi della lista
			printf("%s si e' connesso a %s\n",client->username,client->avversario->username);
			break;
		}
		case 'r':	{ //rifiutato
			ret=send(client->avversario->socket,(void *)&cmd,sizeof(char),0);
			checksend(sizeof(char),14);
			client->stato=0;
			client->avversario->stato=0;
			client->avversario->avversario=NULL;
			client->avversario=NULL;						
			break;
		}
		//non vi e' caso default
	}
}

void quit() {		//CHIUSURA CONNESSIONE
	
	printf("Il client %s si e' disconnesso dal server\n",client->username);
	close(client->socket);
	FD_CLR(client->socket,&master);
	rimozione_elemento_da_lista(client);
}

///////////////////FASE PROVVISORIA
void inizializza1(){	//PRELEVO LA LUNGHEZZA DEL NOME UTENTE
	ret=recv(client->socket,(void*)&lung,sizeof(lung),0);
	errorecv(sizeof(lung),3);
	client->lung=lung;
	client->fase=2;
}

void inizializza2() {	//PRELEVO IL NOME UTENTE
	
	ret=recv(client->socket,(void *)client->username,lung,0);
	errorecv(lung,4);
	client->username[client->lung]='\0';//gestione stringa: carattere di fine string
	client->fase=3;
}

void inizializza3() {	//PRELEVO LA PORTA UDP
	
	char cmd='@';	
	ret=recv(client->socket,(void *)&client->portaUDP,sizeof(client->portaUDP),0);
	errorecv(sizeof(client->portaUDP),5);
	client->portaUDP=ntohs(client->portaUDP);
	rimuovi_da_provvisorio(client);
	if(exist(client->username)) {
		cmd = 'e'; //exists
		ret=send(client->socket,(void*)&cmd,sizeof(cmd),0);
		checksend(sizeof(cmd),15);		
		printf("Il client ha usato un username non valido!\n");
		close(client->socket);
		FD_CLR(client->socket,&master);
		free(client);
		client=NULL;
		return;
	}		
	client->puntatore=utenti_connessi;
	utenti_connessi=client;
	n_utenti_connessi++;
	ret=send(client->socket,(void*)&cmd,sizeof(cmd),0);
	checksend(sizeof(cmd),16);
	client->fase=0;
	printf("%s si e' connesso\n",client->username);
	printf("%s e' libero\n",client->username);
}
	
///////////////////MAIN	
int main(int argc, char* argv[]) {
	int i; 						//contatore per il for
	int addrlen; 				//lunghezza dell'indirizzo
	int des_conn_sock;  		//descrittore del connected socket
	int yes=1;					//
	if(argc!=3) { 				//ret sul numero dei parametri
		printf("Errore durante il passaggio dei parametri\n");
		exit(-1);
		}
	/*##### configurazione server #####*/
	ret=atoi(argv[2]);
	if(ret<1024||ret>65535) {
		printf("Errore nel numero di porta scelto\n");
		exit(1);
	}
	memset(&serveraddr,0,sizeof(serveraddr));
	serveraddr.sin_family=AF_INET;
	serveraddr.sin_port=htons(ret);
	ret=inet_pton(AF_INET,argv[1],&serveraddr.sin_addr.s_addr);
	if(ret==0) {
		printf("indirizzo non valido!\n");
		exit(1);
	}
	printf("Indirizzo: %s (Porta: %s)\n",argv[1],argv[2]);
	listener=socket(AF_INET,SOCK_STREAM,0);
	if(listener==-1){
		printf("errore nella creazione del socket (server)\n");
		exit(1);
	}
	if(setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))==-1) {
		printf("Errore durante la setsockopt()\n");
		exit(1);
	}
	//associamo la porta di ascolto al server
	ret=bind(listener,(struct sockaddr *)&serveraddr,sizeof(serveraddr));
	if(ret==-1){
		printf("errore durante la bind(): forse un server e' gia' attivo\n");
		exit(1);
	}
	ret=listen(listener,10);
	if(ret==-1){
		printf("errore listen()\n");
	exit(1);
	}
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(listener,&master);
	fdmax=listener;
	
	for(;;){
		read_fds=master;
		/*		SELECT		*/
		ret=select(fdmax+1,&read_fds,NULL,NULL,NULL);
		if(ret==-1) {
			printf("errore durante la select()\n");
			exit(1);
		}
		for(i=0;i<=fdmax;i++) {
			////per ogni i definito nell' fd_set read_fds
			if(FD_ISSET(i,&read_fds)) {
				if(i==listener) {				//se i==listener vuol dire che è una NUOVA CONNESSSIONE
					addrlen=sizeof(clientaddr);
					des_conn_sock=accept(listener,(struct sockaddr *)&clientaddr,(socklen_t *)&addrlen);
					if(des_conn_sock==-1)  {
						printf("Errore durante la accept()\n");
						exit(1);
					}
					printf("Connessione stabilita con il client\n");
					FD_SET(des_conn_sock,&master);
					if(des_conn_sock>fdmax) fdmax=des_conn_sock;
					nuovo=malloc(sizeof(struct utente));
					lung=sizeof(clientaddr);
					memset(&clientaddr,0,lung);//mette a 0 i primi lung byte di clientaddr
					getpeername(des_conn_sock,(struct sockaddr *)&clientaddr,(socklen_t *)&lung);//serve per ottenere l'indirizzo di destinazione del socket a cui siamo connessi
					nuovo->indirizzoIP=clientaddr.sin_addr.s_addr;
					nuovo->socket=des_conn_sock;
					nuovo->stato=0;
					nuovo->avversario=NULL;
					nuovo->fase=1;
					nuovo->puntatore=provvisorio;
					provvisorio=nuovo;
					nuovo=NULL;		
				}
				else {							///////CLIENT NOTO
					/////CERCO IL CLIENT IN PROVVISORIO
					client=trova_provvisorio(i);
					if(client!=NULL) {			/////SE IL CLIENT è IN FASE PROVVISORIA PROSEGUI ALLO STEP SUCCESSIVO IN PROVVISORIO
						if(client->fase==3) 
							inizializza3();
						else if(client->fase==2)
							inizializza2();
						else if(client->fase==1)
							inizializza1();
					}
					else {						/////IL CLIENT NON è PROVVISORIO
						client=trova_da_socket(i);
						if(!client) {
							printf("il client non esiste\n");
							exit(1);
						
						}
						
						if(client->fase!=0){	///////CLIENT IN FASE 4 O 5; COMANDO !CONNECT
							switch(client->fase) {
								case 4: {
									connect1();
									break;
								}
								case 5: {
									connect2();
									break;
								}				
							}
						}						
						else{					//FASE=0 -> LEGGO IL COMANDO
							ret=recv(i,(void *)&comando,sizeof(comando),0);
							if(ret==0) {
								quit();
							}							
							else errorecv(sizeof(comando),6);
							switch(comando) {
								case 'r':
								case 'a': {		//A o R -> ACCETATO O RIFIUTATO
									connect3(comando);
									break;
								}
								case 'w': {		//COMANDO WHO QUANTI E CHI SONO CONNESSI
									who();
									break;
								}
								case 'k': case 't':
								case 'd': {		//guarda disconnect del client: t=timeout; d=abbandono; k=finita
									disconnect(comando);
									break;
								}
								case 'c': {		//CONNECT
									client->fase=4;
									break;
								}
								case 'q': {		//QUIT
									quit();
									break;
								}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}



