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

int socketS;						//server socket
int socketC;						//client socket

struct sockaddr_in serveraddr;
struct sockaddr_in clientaddr;
struct sockaddr_in myaddr;
struct timeval timer;

//CLIENT DI RIFERIMENTO
char usernameC[25];
unsigned long indirizzoIPC;
unsigned short portaUDPC;
char simboloC;

//AVVERSARIO
char usernameA[25];
unsigned long indirizzoIPA;
unsigned short portaUDPA;
char simboloA;

int turno=1; 				//avversario 0, cliente di riferimento 1
int n_utenti_connessi;

char comandi[7][12] = {
	"!help",
	"!who",
	"!quit",
	"!connect",
	"!disconnect",
	"!show_map",
	"!hit"
};

char griglia[9];
int n_vuote; 		
char shell;    

fd_set master;
fd_set read_fds;

int fdmax;
int lung;
char help[]="Sono disponibili i seguenti comandi:\n * !help --> mostra l'elenco dei comandi disponibili\n * !who > mostra l'elenco dei client connessi al server\n * !connect nome_client --> avvia una partita con l'utente nome_client\n * !disconnect --> disconnette il client dall'attuale partita intrapresa con un altro peer\n * !quit --> disconnette il client dal server\n * !show_map -> mostra la mappa di gioco\n * !hit num_cell --> marca la casella num_cell (valido solo quando e' il proprio turno)\n";

int fase=0;
int ret;

///////////////////FUNZIONI DI CONTROLLO DELL' ERRORE
void checkrecv(int i,int j){
	if(ret!=i) {	
		printf("Errore durante il comando recv() n%d\n",j);
		exit(1);
	}
}

void checkrecvfrom(int i,int j){
	if(ret!=i) {	
		printf("Errore durante il comando recvfrom() n%d\n",j);
		exit(1);
	}
}

void checksend(int i,int j){
	if(ret!=i){
		printf("Errore durante il comando send() n%d\n",j);
		exit(1);
	}
}	

void checksendto(int i,int j){
	if(ret!=i){
		printf("Errore durante il comando sendto() n%d\n",j);
		exit(1);
	}
}	

///////////////////FUNZIONI DI GIOCO
char tris() {		///valutazione tabella
	if(griglia[0]!=' '&&griglia[0]==griglia[1]&&griglia[1]==griglia[2]) return griglia[2];
	if(griglia[3]!=' '&&griglia[3]==griglia[4]&&griglia[3]==griglia[5]) return griglia[5]; 
	if(griglia[6]!=' '&&griglia[6]==griglia[7]&&griglia[6]==griglia[8]) return griglia[8]; 
	if(griglia[0]!=' '&&griglia[0]==griglia[3]&&griglia[0]==griglia[6]) return griglia[6]; 
	if(griglia[1]!=' '&&griglia[1]==griglia[4]&&griglia[1]==griglia[7]) return griglia[7]; 
	if(griglia[2]!=' '&&griglia[2]==griglia[5]&&griglia[2]==griglia[8]) return griglia[8]; 
	if(griglia[0]!=' '&&griglia[0]==griglia[4]&&griglia[0]==griglia[8]) return griglia[8]; 
	if(griglia[2]!=' '&&griglia[2]==griglia[4]&&griglia[2]==griglia[6]) return griglia[6];
	return 'N';
}

void reset() {
	int i;
	for(i=0;i<9;i++) {
		griglia[i]=' ';
	}
	shell = '#';
	n_vuote=9;
	if(simboloC=='X') turno=1;
	else turno=0;
	return;
}

int cmdtoint (char* cmd) {
	int i;
	for(i=0;i<7;i++) {
		if(strcmp(cmd,comandi[i])==0) 
			return i;
	}
	return -1;
}

void avvio_del_gioco() {
	simboloC='X';
	simboloA='O';
	printf("avvio del gioco\n");
	fase=1;	
	return;	
	}

void ricevo_porta(){
	ret=recv(socketS,(void*)&portaUDPA,sizeof(portaUDPA),0);
	checkrecv(sizeof(portaUDPA),1);
	fase=2;
}

void ricevo_ip(){
	ret=recv(socketS,(void*)&indirizzoIPA,sizeof(indirizzoIPA),0);
	checkrecv(sizeof(indirizzoIPA),2);

	memset(&clientaddr,0,sizeof(clientaddr));
	clientaddr.sin_family=AF_INET;
	clientaddr.sin_port=portaUDPA;
	clientaddr.sin_addr.s_addr=indirizzoIPA;
	portaUDPA=ntohs(portaUDPA);
	reset();
	printf("%s ha accettato la partita\n",usernameA);
	printf("Partita avviata con %s\n",usernameA);
	printf("Il tuo simbolo e': %c\n",simboloC);
	printf("E' il tuo turno:\n");
	fase=0;
	return;
}


///////////////////FUNZIONI CHE IMPLEMENTANO I COMANDI

///////////////////COMANDO WHO
/**//**//**//**//**//**//**//**//**/
void who(){			//COMANDO WHO
	char cmd='w';
	ret=send(socketS,(void *)&cmd,sizeof(char),0);
	checksend(sizeof(char),1);
	fase=3;	
	return;
}

void who0(){		//RICEVO N_UTENTI_CONNESSI
	ret=recv(socketS,(void*)&n_utenti_connessi,sizeof(int),0);
	checkrecv(sizeof(int),3);
	if(n_utenti_connessi==1) 
		printf("1 utente connesso:\n");
	else 
		printf("%d utenti connessi:\n",n_utenti_connessi);
	fase=4;
	return;
}

void who1(){		//RICEVO LUNGHEZZA NOME
	ret=recv(socketS,(void*)&lung,sizeof(int),0);
	checkrecv(sizeof(int),4);
	fase=5;
}

void who2(){		//RICEVO NOME
	char username[25];
	ret=recv(socketS,(void*)username,lung,0);
	checkrecv(lung,5);
	username[lung]='\0';
	printf("%s ",username);
	fase=6;

}

void who3(){		//RICEVO E STATO E CICLO PER OGNI UTENTE
	int stato;	
	ret=recv(socketS,(void*)&stato,sizeof(int),0);
	checkrecv(sizeof(int),6);
	if(stato==0) 
		printf("libero\n");
	else 
		printf("occupato\n");
	n_utenti_connessi--;
	if(n_utenti_connessi==0) 
		fase=0;
	else 
		fase=4;		
	return;
}
/**//**//**//**//**//**//**//**//**/

void connect1() {
	int lung;
	char cmd='c';
	if(strcmp(usernameC,usernameA)==0) {
		printf("L'utente che hai scelto sei tu! \n");
		return;
	}
	ret=send(socketS,(void *)&cmd,sizeof(cmd),0);
	checksend(sizeof(cmd),2);
	lung=strlen(usernameA);
	ret=send(socketS,(void *)&lung,sizeof(lung),0);
	checksend(sizeof(lung),3);
	ret=send(socketS,(void *)usernameA,lung,0);
	checksend(lung,4);
	printf("in attesa di risposta\n");
	
	ret=recv(socketS,(void*)&cmd,sizeof(char),0);
	checkrecv(sizeof(char),7);
	switch(cmd) {		// le risposte arrivano dalle connect2 e connect3 
		case 'i': {
			printf("Impossibile connettersi a %s: utente inesistente. \n",usernameA);
			break;
		}
		case 'a': {
			avvio_del_gioco();
			break;
		}
		case 'b': {
			printf("Impossibile connettersi: l'utente e' occupato.\n");
			break;
		}
		case 'r': {
			printf("Impossibile connettersi a %s: l'utente ha rifiutato la partita.\n",usernameA);
			break;
		}
	}
	return;
}

void show_map(){
            printf("\n");
            printf("   %c | %c | %c \n",*(griglia+6),*(griglia+7),*(griglia+8));
            printf("  ---+---+---\n");
            printf("   %c | %c | %c \n",*(griglia+3),*(griglia+4),*(griglia+5));
            printf("  ---+---+---\n");   
            printf("   %c | %c | %c \n",*(griglia),*(griglia+1),*(griglia+2));
	     printf("\n");
          
}


void disconnect(int end){//2 timeout, 1 abbandonato, 0 finita
	char cmd;
	if(turno==0 && end==1){
		printf("non e' il tuo turno\n");
		return;	
	}
	switch (end) {
		case 0: {
			cmd='k';
			show_map();
			break;
			}
		case 1: {
			cmd='d';
			printf("Ti sei arreso. HAI PERSO\n");
			ret=sendto(socketC,(void *)&cmd,sizeof(char),0,(struct sockaddr*)&clientaddr, (socklen_t)sizeof(clientaddr));
			checksendto(sizeof(char),3);
			break;
		}
		case 2: {
			cmd='t';			
			printf("TIMEOUT!! HAI PERSO\n");
			ret=sendto(socketC,(void *)&cmd,sizeof(char),0,(struct sockaddr*)&clientaddr, (socklen_t)sizeof(clientaddr));
			checksendto(sizeof(char),4);
			break;
		}
	}
	printf("Disconnesso da %s \n",usernameA);
	ret=send(socketS,(void *)&cmd,sizeof(char),0);
	checksend(sizeof(char),5);
	shell='>';
	memset(&clientaddr,0,sizeof(clientaddr));
	return ;
}


void quit(){
	char cmd;
	cmd='q'; 
	if(shell=='#') {
		disconnect(1);
		if(turno==0) 
			return;	
	}
	ret=send(socketS,(void *)&cmd,sizeof(char),0);
	checksend(sizeof(char),6);
	close(socketC);
	close(socketS);
	printf("Client disconnesso correttamente\n");
	exit(0);
}

void hit(int cell) {
	char ris;
	char cmd='h';
	if(shell=='>') {
		printf("comando valido solo in partita!\n");
		return;
	}
	if(!turno) {
		printf("comando valido solo durante il proprio turno!\n");
		return;
	}
	if(griglia[cell]!=' ') {
		printf("la cella %d e' gia' occupata!\n",cell+1);
		return;
	}
	griglia[cell]=simboloC; 
	n_vuote--;
	turno=0; 
	ret=sendto(socketC,(void *)&cmd,sizeof(char),0,(struct sockaddr*)&clientaddr, (socklen_t)sizeof(clientaddr));
	checksendto(sizeof(char),1);
	cell=htonl(cell); 
	ret=sendto(socketC,(void *)&cell,sizeof(int),0,(struct sockaddr*)&clientaddr, (socklen_t)sizeof(clientaddr));
	checksendto(sizeof(int),2);
	ris=tris();	
	if(ris==simboloC) {
		printf("HAI VINTO!!\n");
		disconnect(0); 
	}
	else 
		if(ris!=simboloC && ris!=simboloA && n_vuote==0) {
			printf("PAREGGIO!!\n");
			disconnect(0);
		}
	else
		printf("E' il turno di %s\n",usernameA);
	return;
}

void hit_received(int cell){
	char ris;
	griglia[cell]=simboloA;
	n_vuote--;
	turno=1;
	printf("%s ha marcato la cella numero %d\n",usernameA, cell+1);
	ris=tris();	
	if(ris==simboloA) {
		printf("HAI PERSO!!\n");
		disconnect(0);
	}
	else if(ris!=simboloC && ris!=simboloA && n_vuote==0) {
		printf("PAREGGIO!!\n");
		disconnect(0);
	}
	else printf("E' il mio turno:\n");
	return;
}

///////////////////FUNZIONI UTILITY
void leggi_comando_in_input() {
	char cmd[12];
	int cell;
	fflush(stdin); //per eliminare dal buffer eventuali altri caratteri
	scanf("%s",cmd);
	switch(cmdtoint(cmd)) {
		case 0: {		//help
			printf("%s",help);
			break;
		}
		case 1: { 		//who
			who(); 
			break;
		}
		case 2: { 		//quit
			quit(); 
			break;
		}
		case 3: { 		//connect
			scanf("%s",usernameA);
			if(shell=='>') 
				connect1(); 
			else 
				printf("sei gia' in una partita\n");	
			break;
		}
		case 4: { 		//disconnect
			if(shell=='#') 
				disconnect(1); 
			else 
				printf("Non sei in una partita\n"); 
			break;
		}
		case 5: { 		//showmap
			if(shell=='#') 
				show_map(); 
			else 
				printf("Non sei in una partita\n"); 
			break;
		}
		case 6: { 		//hit
			scanf("%d",&cell); 
			if(cell<1||cell>9) {
				printf("Numero di casella non valido!\n"); 
				return;
			}
			hit(cell-1); 
			break;
		}
		default: 
			printf("Il comando non esiste\n");
	}
	return;
}

void connetti_al_server(char* addr, int port) {
	memset(&serveraddr,0,sizeof(serveraddr));

	ret=inet_pton(AF_INET, addr, &serveraddr.sin_addr.s_addr);
	if(ret==0) {
		printf("indirizzo non valido\n");
		exit(1);
	}

	if(port<1024||port>65535) {
		printf("Numero di porta inserito non valido!\n");
		exit(1);
	}
	serveraddr.sin_port=htons(port);
	socketS=socket(AF_INET,SOCK_STREAM,0);
	if(socketS==1) {
		printf("Errore durante la socket()\n");
		exit(1);
	 }
	 serveraddr.sin_family=AF_INET;
	 ret=connect(socketS,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
	 if(ret==-1) {
		printf("Errore durante la connect(). Probabilmente il server non e' raggiungibile.\n");
		exit(1);
	}
	return;
}

///////////////////ERRORE COL SERVER
void gestisci0(){							
	ret=recv(socketS,(void*)&lung,sizeof(int),0);
	if(ret==0) { //il server si e' disconnesso
		printf("Il server ha chiuso la connessione.\n");
		fflush(stdout);
		exit(1);
	}
	checkrecv(sizeof(int),8);
	memset(usernameA,0,25);
	fase=7;
}	

void gestisci1(){
	char ris;
	char cmd;
	fase=0;
	ret=recv(socketS,(void*)usernameA,lung,0);
	checkrecv(lung,9);
	usernameA[lung]='\0';
	printf("%s ti ha chiesto di giocare!\n",usernameA);
	do {
		scanf("%c",&ris); //
		printf("Accettare la richiesta? (y|n): ");
		fflush(stdin);
		scanf("%c",&ris);
	}
	while(ris!='y'&& ris!='n');
	if(ris=='y') {
		simboloC='O';
		simboloA='X';
		turno=0;
		printf("Richiesta di gioco accettata\n");
		printf("Il tuo simbolo e': %s\n",&simboloC);
		printf("E' il turno di %s\n",usernameA);
		cmd='a';
		ret=send(socketS,(void *)&cmd,sizeof(cmd),0);
		checksend(sizeof(cmd),7);
		reset();
	}
	else {
		printf("Richiesta di gioco rifiutata\n");
		cmd='r';
		ret=send(socketS,(void*)&cmd,sizeof(cmd),0); 
		checksend(sizeof(cmd),8);
	}
	return;
}



/////////////////INIZIO DEL MAIN

int main(int argc,char* argv[]){
	int i;
	char cmd;
	int addrlen;
	int casella;
	unsigned int temp;
	unsigned short port_tmp;


	struct timeval *time;
	
	if(argc!=3) {
		printf("Errore nel passaggio dei parametri\n");
		return -1;
	}
	///////////////////CONNESSIONE AL SERVER
	//il client si connette al server
	connetti_al_server(argv[1],atoi(argv[2]));
	printf("Connessione al server %s (porta %s) effettuata con successo\n",argv[1],argv[2]);
	//chiede i dati al client (porta UDP e username)
	printf("Inscerisci il tuo nome: ");
	scanf("%s",usernameC);
	//invio lunghezza username al server
	lung=strlen(usernameC);
	ret=send(socketS,(void*)&lung,sizeof(lung),0);
	checksend(sizeof(lung),10);
	//invio username al server
	ret=send(socketS,(void*)usernameC,lung,0);
	checksend(lung,11);	//invio porta al server
	do {
		printf("inserisci la porta UDP di ascolto: ");
		scanf("%d",&temp);
		if(temp<1024||temp>65535) {
			printf("Numero della porta errato\n"); 
			temp=1;
		}
		else {
			portaUDPC=(unsigned short)temp; 
			temp=0;
		}	
	}
	while(temp);
	port_tmp=htons(portaUDPC);
	ret=send(socketS,(void*)&port_tmp,sizeof(port_tmp),0);
	checksend(sizeof(port_tmp),11);
	ret=recv(socketS,(void*)&cmd,sizeof(cmd),0);		//SE RICEVO è IL NOME è GIà IN USO
	checkrecv(sizeof(cmd),10);
	if(cmd=='e') {
		printf("Username gia' in uso\n");
		exit(1);					//Per semplicità esco dal programma
	}
	
	//SOCKET UDP
	socketC=socket(AF_INET,SOCK_DGRAM,0);
	if(socketC==-1) {
		printf("Errore durante la socket()\n");
		exit(1);
	}
	
	memset(&myaddr,0,sizeof(myaddr));
	myaddr.sin_family=AF_INET;
	myaddr.sin_port=htons(portaUDPC);
	myaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	
	ret=bind(socketC,(struct sockaddr*)&myaddr,sizeof(myaddr));
	if(ret==-1){
		printf("Errore durante la bind(): forse la porta che hai scelto e' occupata\n");
		quit();
	}

	FD_ZERO(&master);
	FD_SET(socketS,&master);
	FD_SET(socketC,&master);
	FD_SET(0,&master);
	fdmax=(socketS>socketC)?socketS:socketC;
	shell='>';
	printf("%s",help);
	while(1) {
		read_fds=master;
		timer.tv_sec=60;
		timer.tv_usec=0;		
		time=&timer;
		if(shell=='>') time=NULL;
		if(turno==0) time=NULL;
//		if(shell=='#') printf("entro select\n");		
		if(fase==0) { 
			printf("%c",shell); 
			fflush(stdout);
		}
		
		ret=select(fdmax+1,&read_fds,NULL,NULL,time); 
//		if(shell=='#') printf("esco select\n");
		if(ret==-1) {
			printf("Errore durante la select()\n");
			exit(1);
		}
		
		if(ret==0) 					////TIMEOUT
			disconnect(2);
		else 		//NO TIMEOUT
			for(i=0;i<=fdmax;i++) {
				if( FD_ISSET(i,&read_fds) ) { 	//descrittore pronto
					if(i==0) { 					//descrittore pronto e' stdin 
						//leggo comando input
						leggi_comando_in_input();
					}
					if(i==socketS) { 			//descrittore pronto e' server
						if(fase==1) 
							ricevo_porta();
						else 
							if(fase==2) 
								ricevo_ip();					
						else 
							if(fase==3) 
								who0();
						else 
							if(fase==4) 
								who1();
						else 
							if(fase==5) 
								who2();
						else 
							if(fase==6) 
								who3();
						else 
							if(fase==7) 
								gestisci1();					
						else 
							gestisci0();			
					}
					if(i==socketC) {			//descrittore pronto è client
						if(fase==8) {	//ricevo hit
							fase=0;						
							addrlen=sizeof(clientaddr);
							ret=recvfrom(socketC,(void*)&casella,sizeof(int),0,(struct sockaddr*)&clientaddr,(socklen_t *)&addrlen);
							checkrecvfrom(sizeof(int),2);
							casella=ntohl(casella);
							hit_received(casella);
						}
						else {			//ricevo un comando
							ret=recvfrom(socketC,(void*)&cmd,sizeof(cmd),0,(struct sockaddr*)&clientaddr,(socklen_t *)&addrlen);
							switch(cmd) {
								case 'h': {		//HIT
									fase=8;
									break;
								}
								case 'd': { //disconnessione dell'avversario
									printf("%s si e' arreso. HAI VINTO!\n",usernameA);	
									//close(socketC);
									//FD_CLR(socketC,&master);
									shell='>';
									memset(&clientaddr,0,sizeof(clientaddr));
									break;
								}					
								case 't': { //disconnessione dell'avversario
									printf("timeout per %s. HAI VINTO!\nDisconnesso da %s\n",usernameA,usernameA);	
									//close(socketC);
									//FD_CLR(socketC,&master);
									shell='>';
									memset(&clientaddr,0,sizeof(clientaddr));
									break;
								}
							}
						}
					}		
				}
			}
	}	
	return 0;
}
