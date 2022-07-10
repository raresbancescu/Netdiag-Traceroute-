#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include <sqlite3.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<fcntl.h>
#include<pthread.h>
#include<signal.h>
#include<netdb.h>
#include <time.h>
#include <sys/wait.h>
#include<netinet/ip.h>
#include<netinet/ip_icmp.h>


#define PORT 2909
// Threaduri
struct Thread
{
  pthread_t id;
  int count;
};
struct Thread vector_thread[20];
pthread_mutex_t mlock=PTHREAD_MUTEX_INITIALIZER;
int nrthread=20;
//

int sd;
// Traceroute conflicte pachete
int traceroute_rand=0;
int inchidere=0;

sqlite3 *database;




void citire_comanda(int sd,char primit[10000]) //citire comenzi de la client
{
  int nr_primite;
 
  read(sd,&nr_primite,sizeof(int));

    read(sd,primit,nr_primite-1);
}

void trimitere_comanda2(int sd, char primit[10000]) //pentru traceroute
{
  int nr_primite=strlen(primit);

 //write(sd,&nr_primite,sizeof(int));
  
  write(sd,primit,nr_primite);
  
}
void trimitere_comanda(int sd, char primit[10000])// trimitere comenzi necontinue
{
  int nr_primite=strlen(primit);

 write(sd,&nr_primite,sizeof(int));
  
  write(sd,primit,nr_primite);
  
}
int hostname_to_ip(char *nume_host, char* ip_host) // DNS obtinem ip ul unui hostname
{
  struct hostent *x;
  struct in_addr **lista_addr;
  int i;
  
  if ((x=gethostbyname(nume_host))==NULL)
  {
    //printf("Nu exista acest nume de host");
    return 0;
  }

  lista_addr=(struct in_addr **) x->h_addr_list;

  for (i=0;lista_addr[i]!=NULL;i++)  // pentru un hostname pot exista mai multe adrese ip, o vom selecta pe prima
  {
  strcpy(ip_host,inet_ntoa(*lista_addr[i]));
  return 1;
  }
  return 1;
}



      

unsigned short check_sum(unsigned short *mesaj, int cuvinte) //verificare loss
{
  unsigned long sum;
  for (sum=0;cuvinte>0;cuvinte--)
  {
    sum=sum+ *mesaj++;
  }
    sum=(sum>>16) + (sum & 0xffff);
    sum=sum+(sum>>16);
     
  return ~sum;
}


void open_database()
{
  int r=sqlite3_open("database.db",&database);
  if (r!=SQLITE_OK)
  {
    printf("Nu se poate deschide baza de date");
    sqlite3_close(database);
    
  }
  
}

void close_database()
{
  int r=sqlite3_close(database);
  if (r!=SQLITE_OK)
  {
    printf("Nu se poate inchide baza de date");
  }
}


void cerere_mesaj_bd(int id_thread, char*destinatie,int sd,int interogare, int unde) // face selectul in functie de parametrii dati
{
  
  char *mesaj_eroare=0;
  char comanda[10000];
  char numar[100];
   int coloane=0;
  char mesaj_intoarcere[10000];
  strcpy(mesaj_intoarcere,"");
  sprintf(numar,"%d",id_thread);
  //alcatuim fraza select
  if (interogare==0) //ip ,ttl si mesaj
  {
  strcpy(comanda,"SELECT hopnumber,adresaip,timp,mesajhop FROM traceroute WHERE idthread=");
  coloane=4;
  }
  if (interogare==1) //+ nume host
  {
    strcpy(comanda,"SELECT hopnumber,numehost,timp,mesajhop,packetloss  FROM traceroute WHERE idthread="); //doar nume host
    coloane=4;
  }
  if (interogare==2) // adresa ip
  {
    strcpy(comanda,"SELECT hopnumber,adresaip,timp,mesajhop,packetloss FROM traceroute WHERE idthread="); //doar ip
    coloane=5;
  }
  if (interogare==3) //toate
  {
    strcpy(comanda,"SELECT hopnumber,numehost,adresaip,timp,mesajhop,packetloss FROM traceroute WHERE idthread="); //ambii parametrii afisam tot
    coloane=6;
  }
  strcat(comanda,numar);
  strcat(comanda," ");
  strcat(comanda,"and trim(adresaceruta) like '");
  strcat(comanda,destinatie);
  strcat(comanda,"';");
  
  sqlite3_stmt *stat=NULL;
  int r=sqlite3_prepare_v2(database,comanda,-1,&stat,NULL);
  //verificare
  if (r!=SQLITE_OK)
  {
    printf("Eroare la trimiterea mesajelor din baza de date");
  }

 
  r=sqlite3_step(stat); //parcurgem pas cu pas interogarea
  int tip_coloana;
  int randuri=0;
  while (r!=SQLITE_DONE && r!=SQLITE_OK) 
  {
    randuri++;
    
    for (int i=0;i<coloane;i++)
    {
      tip_coloana=sqlite3_column_type(stat,i);
      

      if (tip_coloana=SQLITE_TEXT)
      {
        const char *value=sqlite3_column_text(stat,i);
        strcat(mesaj_intoarcere,value);
        strcat(mesaj_intoarcere," ");
       
      }
      
    }
    strcat(mesaj_intoarcere,"\n");
    r=sqlite3_step(stat);

  }
 
  r=sqlite3_finalize(stat);
  sqlite3_free(mesaj_eroare);
   
   
  if (unde==0)
  {
  if (sizeof(mesaj_intoarcere)<3)
  {
    trimitere_comanda2(sd,"Nu exista mesaje in bd");
    memset(mesaj_intoarcere,0,sizeof(mesaj_intoarcere));
  }
  else
  {
 trimitere_comanda2(sd,mesaj_intoarcere);
 memset(mesaj_intoarcere,0,sizeof(mesaj_intoarcere));
  }
  }
  else
  {
    trimitere_comanda(sd,mesaj_intoarcere);
  }
 
   
  
  memset(numar,0,sizeof(numar));
}

void creare_mesaj_bd(int id,char *adresa_ceruta, int hop, char *adresa_ip, char *nume_host,double timp, char *mesaj_hop,double packetloss)
{
  char mesaj[1000];
  char num1[1000];
  char num2[1000];
  char num3[1000];
  char num4[1000];
  sprintf(num1,"%d",id);
  sprintf(num2,"%d",hop);
  sprintf(num3,"%lf",packetloss);
  sprintf(num4,"%lf",timp);
  strcpy(mesaj,"INSERT INTO traceroute VALUES (");
   strcat(mesaj,num1);
   strcat(mesaj,",");
    strcat(mesaj,"'");
   strcat(mesaj,adresa_ceruta);
   strcat(mesaj,"'");
   strcat(mesaj,",");
    strcat(mesaj,num2);
    strcat(mesaj,",");
    strcat(mesaj,"'");
  strcat(mesaj,adresa_ip);
  strcat(mesaj,"'");
    strcat(mesaj,",");
    strcat(mesaj,"'");
  strcat(mesaj,nume_host);
  strcat(mesaj,"'");
    strcat(mesaj,",");
    strcat(mesaj,"'");
    strcat(mesaj,num4);
    strcat(mesaj,"'");
    strcat(mesaj,",");
    strcat(mesaj,"'");
  strcat(mesaj,mesaj_hop);
  strcat(mesaj,"'");
    strcat(mesaj,",");
  strcat(mesaj,num3);
  strcat(mesaj,");");

  char *mesaj_eroare=0;
  char *sql_statement=mesaj;
  int r=sqlite3_exec(database,sql_statement,0,0,&mesaj_eroare);

  if (r!=SQLITE_OK)
  {
    printf("Eroare la inserarea datelor %s\n",mesaj_eroare);
  }
  memset(num1,0,sizeof(num1));
  memset(num2,0,sizeof(num2));
  sqlite3_free(mesaj_eroare);
}

void traceroute ( char *destinatie, int id_thread, int t)
{
  // alcatuim socketul de tip raw, vom utiliza protocolul ICMP
  int soc=socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  char mesaj[4096];
  //pentru baza de date
  char mesaj_hop[1000];
  char nume_host[1000];
  char ip_host[1000];
  int nr_checksum;
  int return_status;
  int nr_incercari=0;
  int reincercare=0;
  int efectuare=1;
  double loss;
  
  //
  int v=1;
  int hop=1;
  const int *par=&v;
  struct ip * header_ip=(struct ip*) mesaj;

  //setam optiunea socketului de a avea packet customizabil, utilizam IP si nu lasam SO sa completeze campurile
   if (setsockopt(soc,IPPROTO_IP,IP_HDRINCL,par,sizeof(int))<0) 
   {
     printf("Nu se poate creea socketul; Trebuie sudo;");
   }
   

//setam optiunea socketului de a avea un timp limitat de asteptare a pachetelor pentru a semnala daca un router nu raspunde 

   struct timeval timevalue;
  timevalue.tv_sec = 1;  
setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&timevalue,sizeof(struct timeval));
   
   
   //aflam ip ul primit de la client
   char ip_destinatie[100];
   if (0==(hostname_to_ip(destinatie,ip_destinatie)))
   {
     efectuare=0;
   }


  if (efectuare==1)
  {
   struct sockaddr_in adresa;
   adresa.sin_port = htons(7);
   adresa.sin_family=AF_INET;
   inet_pton(AF_INET,ip_destinatie, &(adresa.sin_addr));
  int ok=1;

  struct hostent *nume;
      struct in_addr ip_conversie;

      // facem stergerea rutei anterioare 
      char *mesaj_eroare=0;
      char statement[1000];
      sprintf(statement, "DELETE FROM traceroute WHERE idthread = %d AND trim(adresaceruta) like \'%s\';", id_thread, destinatie);
        
        int r=sqlite3_exec(database,statement,0,0,&mesaj_eroare);
        
        if (r!=SQLITE_OK)
        {
          printf("Nu s au sters");
        }

   // incepem traceroute     
  while(ok)
  {
    reincercare=0;

    //initializari header pachet ip
    header_ip->ip_hl=5;
    header_ip->ip_v=4;
    header_ip->ip_tos=0;
    header_ip->ip_len=20+8;
    header_ip->ip_id = 10000;
    header_ip->ip_off =0;
    header_ip->ip_ttl=hop;
    header_ip->ip_p=IPPROTO_ICMP;
    // sursa pachetului
    inet_pton(AF_INET,"192.168.0.100",&(header_ip->ip_src));
    //destinatia pachetului
    inet_pton(AF_INET,ip_destinatie,&(header_ip->ip_dst));
    //checksum
    header_ip->ip_sum=check_sum((unsigned short *) mesaj,9);

    //headerul pentru protocolul icmp
 struct icmphdr *header_icmp=(struct icmphdr*) (mesaj+20);
   
    header_icmp->type=ICMP_ECHO; //vrem sa trimitem mesaje de tip echo
    header_icmp->code=0;
   header_icmp->checksum=0;
    header_icmp->un.echo.id=0;
    header_icmp->un.echo.sequence=hop+1;
     header_icmp->checksum=check_sum((unsigned short *) (mesaj+20),4); // verificam doar pentru pachetul mare loss ul
    
    //trimitem pachetul
    clock_t timp;
    timp=clock();
    sendto(soc,mesaj,sizeof(struct ip)+ sizeof(struct icmphdr),0, (struct sockaddr*) & adresa, sizeof adresa);
   
    //primim pachetul
    char mesaj_primit[4096]={0};
    struct sockaddr_in adresa2;
    int lungime= sizeof(struct sockaddr_in);
    return_status=recvfrom(soc,mesaj_primit,sizeof(mesaj_primit),0, (struct sockaddr*) &adresa2, &lungime);
    fflush(stdout);
    
    
    // vom folosi pentru analiza retelei
    struct icmphdr *verificare = (struct icmphdr *) (mesaj_primit+20);
    
    // eroare
    if (verificare->type!=0 && adresa2.sin_port==adresa.sin_port)
    {
      
    
      inet_aton(inet_ntoa(adresa2.sin_addr),&ip_conversie);
      nume=gethostbyaddr((const void *)&ip_conversie,sizeof ip_conversie,AF_INET);
      
      if (verificare->type==11) //s a atins un nod cu nr de hop
      {
        if (verificare->code==1)
        {
          strcpy(mesaj_hop,"Nu s a reusit reasamblarea fragmentelor"); // reasamblare nereusita
        }


        if (verificare->code==0) //ttl a expirat
        {
        strcpy(mesaj_hop,"Hop bun");
        }
      
      }

      if (verificare->type==3) //destination unreachable
      {

        strcpy(mesaj_hop,"Destinatia aceasta nu poate fi atinsa; Se incearca la distanta mai mare");
      }
     
     
    }

    else  // am primit echo reply de la adresa destinatie
    {
      //extragem adresa si numele

      inet_aton(inet_ntoa(adresa2.sin_addr),&ip_conversie);
      nume=gethostbyaddr((const void *)&ip_conversie,sizeof ip_conversie,AF_INET);
      if (strcmp(ip_destinatie,inet_ntoa(adresa2.sin_addr))==0)
      {
     
      strcpy(mesaj_hop,"S a atins destinatia ceruta");
    
      ok=0;
      }
    }
  
  
  if (hop>=t)
  {
    strcpy(mesaj_hop,"S-a atins numarul maxim de hopuri");
    ok=0;
  }

  // alcatuim insert ul pentru baza de date

  if (nume)
  {
    strcpy(nume_host,nume->h_name);
  }
  else
  {
    strcpy(nume_host,"Nu are nume");
  }


   loss=(header_icmp->checksum*100)/(verificare->checksum+0.001);
    if (loss>100)
    {
      loss=100;
    }
    loss=100-loss;
   timp=clock()-timp;
   double timp_final=(((double)timp)/CLOCKS_PER_SEC)*1000;
   
    strcpy(ip_host,inet_ntoa(adresa2.sin_addr));
    nr_checksum=verificare->checksum;
   
    if (return_status==-1)
    {
      strcpy(ip_host,"Waiting for reply");
      loss=100;
    }
    if (strcmp(ip_host,"Waiting for reply")!=0 || hop!=1)
    creare_mesaj_bd(id_thread,destinatie,hop,ip_host,nume_host,timp_final,mesaj_hop,loss);

  memset(&mesaj,0,sizeof(mesaj));
  memset(&mesaj_primit,0,sizeof(mesaj_primit));
  memset(&mesaj_hop,0,sizeof(mesaj_hop));
  memset(&verificare,0,sizeof(verificare));
    if (reincercare!=1)
    {
    hop++;
    }
  }
  }
}




void executie_comenzi (int c1, int id_Thread)
{
  //initializari optiuni
    char primit[10000]; //mesaj primit de la client
    char trimit[10000];
    char cuv1[100];
    char cuv2[100];
    char cuv3[100];
    char cuv4[100];
    char cuv5[100];
    char dest[100];
    int nr_primite;
    int ok;
    int continuare_traceroute=1;
    int optn,opti,t;
    int interogare;
    inchidere=0;
    optn=0;
    opti=0;
    while (inchidere==0)
    {
      
        citire_comanda(c1,primit);
 
  
  if (strstr(primit,"help"))
  {
     memset(primit,0,10000);
 memset(trimit,0,10000);
    strcpy(trimit,"Pentru a utiliza programul introduceti traceroute [optiuni] adresa pentru care doriti sa afisati ruta.\n");
    strcat(trimit,"Daca doriti sa primiti informatii minime despre ruta introduceti traceroute si numele hostului (sau adresa IP)\n");
    strcat(trimit,"Daca doriti sa primiti doar numele hostului utilizati optiunea -n\n");
    strcat(trimit,"Daca doriti sa primiti doar adresa ip utilizati optiunea -i \n");
    strcat(trimit,"Daca doriti sa afisati toate informatiile utilizati ambele optiuni \n");
    strcat(trimit,"Daca doriti sa specificati un numar maxim de hopuri introduceti dupa numele hostului numarul de hopuri\n");
    strcat(trimit,"Daca doriti sa revedeti ruta executati ruta si nume\n");
     strcat(trimit,"Exemplu de comanda: traceroute -n -i google.com 10\n");
    trimitere_comanda(c1,trimit);
    memset(primit,0,10000);
 memset(trimit,0,10000);
  inchidere=0;
  }

   else if (strstr(primit,"traceroute"))
  {
    
    strcpy(cuv1," ");
    strcpy(cuv2," ");
    strcpy(cuv3," ");
    strcpy(cuv4," ");
    strcpy(cuv5," ");
    char *prim;
    int nr=1;
    int t;
    prim=strtok(primit," ");
    strcpy(cuv1,prim);
    while (prim!=NULL)
    {
      //verificari de parametrii primiti 
      if (strstr(prim,"-n"))
      {
        optn=1;
      }
       if (strstr(prim,"-i"))
      {
        opti=1;
      }
       ok=1;
      for (int i=0;i<strlen(prim)-1;i++)
      {
      
        if (prim[i]>'9' || prim[i]<'0')
        {
          ok=0;
        }
      }
      if (nr==1)
      {
        strcpy(cuv1,prim);
      }
     else if (nr==2)
      {
        strcpy(cuv2,prim);
      }
     else if (nr==3)
      {
        strcpy(cuv3,prim);
      }
      else if (nr==4)
      {
        strcpy(cuv4,prim);
      }
      else if (nr==5)
      {
        strcpy(cuv5,prim);
      }
      nr++;
      prim=strtok(NULL," ");
      
    }
    
  
   
   if (nr==2)
    {
      trimitere_comanda2(c1,"Numar incorect de parametrii;");
    continuare_traceroute=0;
    }
  else
  {
    

    if (nr==3)
    {
      t=1000;
      interogare=0;
      
     strcpy(dest,cuv2);
     
      
    }

    if (nr==4)
    {
      if (ok==1)
      {
        t=atoi(cuv3);
       
        strcpy(dest,cuv2);
       
        interogare=0;
       
      }
      else
      {
        if (opti==1)
        {
          interogare=2;
        }
        if (optn==1)
        {
          interogare=1;
        }
        t=1000;
        strcpy(dest,cuv3);
       
       
      }
    }

    if (nr==5)
    {
      if (ok==1)
      {
        t=atoi(cuv4);
       if (optn==1)
       {
         interogare=2;
       }
       if (opti==1)
       {
         interogare=1;
       }
       strcpy(dest,cuv3);
      
       
      }
      else //sunt ambii parametrii de optiune
      {
        t=1000;
        interogare=3;
      
      strcpy(dest,cuv4);
       
      }
    }

    if (nr==6)
    {
      t=atoi(cuv5);
      interogare=3;
      strcpy(dest,cuv4);
      
    }
    if (nr==7)
    {
      trimitere_comanda2(c1,"Prea multi parametrii");
        continuare_traceroute=0;
    }
    struct hostent *name;
    if ((name=gethostbyname(dest))==NULL)
    {
      trimitere_comanda2(c1,"Nu exista acest host");
        continuare_traceroute=0;
    }

    // In cazul in care se executa mai multe traceroute, este necesara asteptarea
      if (traceroute_rand==1)
      {
        trimitere_comanda2(c1,"Conflict de pachete; se executa dupa finalizarea altui traceroute\n");
          
      }
      while (traceroute_rand==1)
      {

      }
       sleep(5);
      open_database();
     
      
      int pid;
      if (-1==(pid=fork()))
      {
        printf("Eroare la creare fork");
      }
      system("rm -r fisier.txt");
      system("touch fisier.txt");
      // copil ce executa continuu traceroute
      if (pid==0)
      {
        int ok=1;
        char mesaj;
        int id=0;
        int fd;
        fd=open("fisier.txt",O_RDONLY);
        
        //trimitere_comanda(c1,"Se face traceroute");
        while (ok==1)
        {
          
          if (read(fd,&mesaj,1)!=0)
          {
            ok=0;
            close(fd);
            open_database();
          traceroute(dest,id_Thread,t); 
          exit(1);
          }
          else
          {
          
          open_database();
          traceroute(dest,id_Thread,t);
          cerere_mesaj_bd(id_Thread,dest,c1,interogare,0);
          }
       
          
          
            
          }
        
       
      }
      // am primit de la client semnal de oprire
      else
      {
        int fd2;
        char tt[10];
        traceroute_rand=1;
        strcpy(tt,"stop");
        citire_comanda(c1,primit);
        trimitere_comanda2(c1,"Gata");
        traceroute_rand=0;
        fd2=open("fisier.txt",O_WRONLY|O_NONBLOCK);
        write(fd2,&tt,sizeof(tt));
        close(fd2);
         memset(primit,0,10000);
       memset(trimit,0,10000);
        wait(NULL);
      
      }

   



  

  }

  inchidere=0;
    
  }
  
   
   else if (strstr(primit,"quit"))
   {
     inchidere=1;
     trimitere_comanda(c1,"quit");
   } 
    
   else if (strstr(primit,"ruta"))
   {
     char *cuv;
     char ruta[100];
     char adrr[100];
     cuv=strtok(primit," ");
     strcpy(ruta,cuv);
     int n=0;
     while (cuv!=NULL)
     {
       if (n==0)
       {
         strcpy(ruta,cuv);
       }
       if (n==1)
       {
         strcpy(adrr,cuv);
       }
       n++;
       cuv=strtok(NULL," ");
     }
     //printf("%s",adrr);
     open_database();
     cerere_mesaj_bd(id_Thread,adrr,c1,interogare,1);
     close_database();
     memset(adrr,0,sizeof(adrr));
    inchidere=0;
   } 
    else
    {
    
   
    trimitere_comanda(c1,"Sintaxa gresita\n");
 memset(primit,0,10000);
 memset(trimit,0,10000);
 inchidere=0;
    }
    
   
    }
}

static void *executie_thread(void *arg)
{
  int client;
  struct sockaddr_in from;
  bzero(&from,sizeof(from));


  while (1)
  {
    //mutex
    int length=sizeof(from);
    pthread_mutex_lock(&mlock);
    if ((client=accept(sd,(struct sockaddr *) &from,&length))<0)
    {
      printf("Eroare la accept\n");
      exit(7);
    }
    pthread_mutex_unlock(&mlock);
    // se adauga clientul nou
    vector_thread[(int) arg].count++;
    //executie comenzi
    executie_comenzi(client,(int) arg); // procesam cererea
   
    close(client);
  }
}





void creare_thread(int i)
{
  pthread_create(&vector_thread[i].id,NULL,&executie_thread,(void*)i);
  return; //threadul principal returneaza
}



int main()
{
  struct sockaddr_in server;

  
  if ((sd=socket(AF_INET,SOCK_STREAM,0))==-1)
  {
    printf("Eroare la crearea socketului");
    exit(1);
  }

  int on=1;
  // in cazul in care o adresa este utilizata deja se incearca utilizarea ei
  setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)); 

  bzero(&server,sizeof(server));
  
  server.sin_family=AF_INET;
  server.sin_addr.s_addr=htonl(INADDR_ANY);
  server.sin_port=htons(PORT);

  
  if (bind(sd,(struct sockaddr*)&server,sizeof(struct sockaddr))==-1)
  {
    printf("Eroare la bind");
    exit(2);
  }

  //listen 

  if (listen(sd,2)==-1)
  {
    printf("Eroare la listen");
    exit(3);
  }

  //puteam accepta maxim 20 de clienti, threadurile sunt deja active, asteapta sa primeasca clienti si sa comunice cu ei
  for (int i=0;i<nrthread;i++)
  {
    creare_thread(i); //se creeaza
  }

  while (1)
  {
    pause();
  }

  return 0;
}
