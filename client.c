#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <signal.h>
int port;

    char trimis[10000],primit[10000],citit[10000];
    int length1,length2;
    char ch;

void trimitere_mesaj(int sd,char mesaj[])
{
   int length=strlen(mesaj);
        
        write(sd,&length,sizeof(int));
        write(sd,citit,length-1);
}

int primire_mesaj(int sd,char mesaj[])
{
    int length=strlen(mesaj);
    
        if (read(sd,&length,sizeof(int))==0)
        {
            return 0;
        }
        read(sd,mesaj,length);
        
}
 int sd;
void sighandler(int sig)
{
    trimitere_mesaj(sd,"done");
}
int main(int argc, char* argv[])
{
   
    struct sockaddr_in server;
    int nr=0;
    int ok;
    char buf[10];

    if (argc!=3)
    {
        printf("Eroare nu au fost date toate detaliile");  //adresa server si portul
        exit(1);
    }

    port=atoi(argv[2]);

    if ( (sd=socket(AF_INET,SOCK_STREAM,0))==-1)
    {
        printf("Eroare la creare socket");
        exit(2);
    }

    server.sin_family=AF_INET;
    server.sin_addr.s_addr=inet_addr(argv[1]); // punem in structura adresa ip a serverului
    server.sin_port=htons(port);

    if (connect(sd,(struct sockaddr *)&server,sizeof(struct sockaddr))==-1)
    {
        printf("Eroare la conectarea la server");
        exit(3);
    }    
    printf("Acest program executa traceroute pentru o anumita destinatie\n");
    printf("Pentru instructiuni introduceti help:\n");
 
    
   while (1)
    {
        signal(SIGINT,SIG_IGN);
       ok=1;
        printf("Comanda:");
        fgets(citit,sizeof(citit),stdin);
        citit[strlen(citit)]='\0';
        if (citit[0]!=10)
        {
            
        trimitere_mesaj(sd,citit); 
        if(strstr(citit,"traceroute"))
        {
            signal(SIGINT,sighandler);
        //      memset(primit,0,10000);
        // memset(citit,0,10000);
       while (read(sd,&primit,sizeof(primit))!=0 && ok==1)
       {
          
           if (strstr(primit,"Gata"))
           {
               memset(primit,0,10000);
             memset(citit,0,10000);
           ok=0;
           
           }
           else if (strstr(primit,"Prea multi parametrii;"))
           {
                printf("%s\n",primit);
                trimitere_mesaj(sd,"done");
                ok=0;
           }
           else if (strstr(primit,"Numar incorect de parametrii;"))
           {
                printf("%s\n",primit);
                 trimitere_mesaj(sd,"done");
                ok=0;
           }
           else if (strstr(primit,"Nu exista acest host;"))
           {
                printf("%s\n",primit);
                 trimitere_mesaj(sd,"done");
                ok=0;
           }
           else
           {
               //system("clear");
           printf("%s",primit);
            memset(primit,0,10000);
             memset(citit,0,10000);
           }
       }
       strcpy(primit,"");
       
        }
        else
        {
            primire_mesaj(sd,primit);
             printf("%s",primit);
              
        }
        if (strcmp(primit,"quit")==0)
        {
            exit(1);
        }
       
        
        }
            memset(primit,0,10000);
             memset(citit,0,10000);
    }

    
    return 0;
}