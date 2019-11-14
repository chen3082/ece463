#include "ne.h"
#include "router.h"
#include <time.h>
#include <pthread.h>

pthread_mutex_t lock;
struct pass_args { //struct for the parameters pass to thread function
  int udp_fd;
  int host;
  int router_id;
  FILE * fp;
  struct sockaddr_in ne_addr;
  //socklen_t ne_addrlen = sizeof(ne_addr);
  struct sockaddr_in router_addr;
  //socklen_t router_addrlen = sizeof(router_addr);
}pass_args;

void * receive_from(void *);
void * send_to(void *);

struct nbr_temp {
  int ID;
  int cost;
  long last_update;
  int dead;
} nbr_temp;

struct nbr_temp nbrs[MAX_ROUTERS];
//init time variables
int total_nbrs;
unsigned int update_time = 0;
unsigned int converge_time;
unsigned int start_time;
int converged = 0;
FILE * rlog;

//to keep track of neighbors for updating
//just for reuse code purpose, not actually calling
int _udp_listenfd(int server_port){ 
  int server_socket = -1;
  struct sockaddr_in server_address;
  //create server socket
  if((server_socket = socket(AF_INET, SOCK_DGRAM,0)) < 0){
    printf("Socket create fails\n");
    return -1;
  }
  //check bind addr
  bzero((char *) &server_address, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY); 
  server_address.sin_port = htons((unsigned short)server_port); 
  //bind server socket
  if((bind(server_socket,(struct sockaddr *)&server_address, sizeof(server_address)))<0){
    printf("udp bind Socket fails\n");
    return -1;
  }
  //eliminate address conflict
  return server_socket; 
}

int main(int argc, char **argv) {
  if (argc != 5){
    printf("There should be 5 arguments.\n");
    return 0;
  }
  int router_id, router_port, udp_listenfd;
  int ne_port;
  int i;
  char * ne_host;
  struct pkt_INIT_REQUEST req;
  struct pkt_INIT_RESPONSE respon;
  //check router ID
  router_id = atoi(argv[1]);
  if (router_id < 0 || router_id > (MAX_ROUTERS - 1)){
    printf("Invalid router's ID.\n");
    return 0;
  }
  req.router_id = htonl(router_id);
  //get parameters from bash
  router_port = atoi(argv[4]);
  ne_host = argv[2];
  ne_port = atoi(argv[3]);
  //create log file
  char filename[11];
  sprintf(filename, "router%d.log",router_id);
  rlog = fopen(filename,"w+");
  struct sockaddr_in ne_addr;
  socklen_t ne_addrlen = sizeof(ne_addr);
  struct sockaddr_in router_addr;
  //socklen_t router_addrlen = sizeof(router_addr);

  //create server socket
  if ((udp_listenfd = socket(AF_INET, SOCK_DGRAM,0)) < 0){
    printf("Socket create fails\n");
    return -1;
  }
  //check bind addr
  struct hostent * ne = gethostbyname(ne_host);
  if (ne == NULL){
    printf("host does not exist.\n");
    exit(-1);
  }

  bzero((char *) &ne_addr, sizeof(ne_addr));
  ne_addr.sin_family = AF_INET;
  memcpy((void *)&ne_addr.sin_addr,ne->h_addr_list[0],ne->h_length);
  ne_addr.sin_port = htons((unsigned short)ne_port);

  bzero((char *) &router_addr, sizeof(router_addr));
  router_addr.sin_family = AF_INET;
  router_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  router_addr.sin_port = htons((unsigned short)router_port);
  
  //bind server socket
  if(bind(udp_listenfd,(struct sockaddr *)&router_addr, sizeof(router_addr))<0){
    close(udp_listenfd);
    printf("udp bind Socket fails\n");
    return -1;
  }
  //send and receive initial request
  if (sendto(udp_listenfd, &req, sizeof(req),0,(struct sockaddr *)&ne_addr,ne_addrlen) < 0){
    printf("Packet sent failed\n");
    return EXIT_FAILURE;
  }
  int receive_len;
  if ((receive_len = recvfrom(udp_listenfd, &respon, sizeof(respon),0,(struct sockaddr *)&router_addr,&ne_addrlen)) < 0){
    printf("Packet receive failed\n");
    return EXIT_FAILURE;
  }
  //convert and initial reponse
  ntoh_pkt_INIT_RESPONSE(&respon);
  InitRoutingTbl(&respon, router_id);
  printf("\ninit successful\n");
  PrintRoutes(rlog, router_id);
  //fflush(rlog);
  //finish initial routing table
  start_time = time(NULL);
  pthread_t receive;
  pthread_t send;
  struct pass_args arguments;
  //send update
  //receive update
  //first construct the argument passed to threads
  arguments.udp_fd = udp_listenfd;
  arguments.ne_addr = ne_addr;
  arguments.router_id = router_id;
  total_nbrs = respon.no_nbr;
  arguments.fp = rlog;
  //hton_pkt_RT_UPDATE(&arguments.update);
  //transfer info into nbrs
  for (i = 0; i < respon.no_nbr; i++){
    nbrs[i].last_update = time(NULL);
    nbrs[i].ID = respon.nbrcost[i].nbr;
    nbrs[i].cost = respon.nbrcost[i].cost;
    nbrs[i].dead = 0;
  }
  converge_time = time(NULL);
  //creating thread
  if (pthread_create(&send, NULL, send_to,(void *)&arguments)) {
    perror("Error creating thread for sending update");
    return EXIT_FAILURE;
  }
  if (pthread_create(&receive, NULL, receive_from,(void *)&arguments)) {
    perror("Error creating thread for receivinh update");
    return EXIT_FAILURE;
  }
  pthread_join(receive,NULL);
  pthread_join(send,NULL);
  return 0;
}

void * receive_from(void *args){
  int  update;
  update = 0;
  int i;
  struct pass_args * arg = args;
  struct pkt_RT_UPDATE update_pkt;
  //socklen_t ne_addrlen_temp = (socklen_t)sizeof(arg -> router_addr);
  while (1) {
    if (recvfrom(arg -> udp_fd, &update_pkt, sizeof(update_pkt),0,NULL,NULL) < 0){
      printf("Packet receive failed\n");
      //return EXIT_FAILURE;
    }
    ntoh_pkt_RT_UPDATE(&update_pkt);
    pthread_mutex_lock(&lock);
    for (i = 0; i < total_nbrs; i++){
      if (update_pkt.sender_id == nbrs[i].ID){
	nbrs[i].last_update = time(NULL);
	nbrs[i].dead = 0;
	break;
      }
    }
    update = UpdateRoutes(&update_pkt,nbrs[i].cost,arg -> router_id);
    if (update){
      PrintRoutes(rlog, arg -> router_id);
      fflush(rlog);
      converged = 0;
      converge_time = time(NULL);
    }

    pthread_mutex_unlock(&lock);
  }
  return NULL;
}


void * send_to(void *args){
  int i;
  //int sendto_len, sendto_size;p;
  struct pass_args * arg = args;
  struct pkt_RT_UPDATE update_pkt;
  socklen_t ne_addrlen_temp = sizeof(arg -> ne_addr);
  while (1){
    pthread_mutex_lock(&lock);
    if((time(NULL) - update_time) >= UPDATE_INTERVAL){
      for(i = 0; i < total_nbrs; i++){
	bzero(&update_pkt, sizeof(update_pkt));    
	ConvertTabletoPkt(&update_pkt, arg -> router_id);
	update_pkt.dest_id = nbrs[i].ID;
	hton_pkt_RT_UPDATE(&update_pkt);
	if (sendto(arg -> udp_fd, &update_pkt, sizeof(update_pkt),0,(struct sockaddr *)&(arg -> ne_addr),ne_addrlen_temp) < 0){
	  printf("Packet send failed\n");
	  //return EXIT_FAILURE;
	}// for small if
	update_time = time(NULL);
      }
    }//for else and big if
    for (i = 0; i < total_nbrs; i++){
      if((time(NULL) - nbrs[i].last_update) > FAILURE_DETECTION){
	//the router A marks the link to B as inactive
	if (nbrs[i].dead == 0){
		UninstallRoutesOnNbrDeath(nbrs[i].ID);
		converged = 0;
        	PrintRoutes(rlog, arg -> router_id);
		converge_time = time(NULL);
	}
	nbrs[i].dead = 1;//if router is dead, do not print anymore
      }// FOR FAILURE_dETECTION
    }// for for loop
    if ((time(NULL) - converge_time) >= CONVERGE_TIMEOUT){
      if (converged == 0){
	printf("Converged\n");
	fprintf(rlog,"%d:Converged\n",(int)time(NULL) - (int)start_time);
	fflush(rlog);
	converge_time = time(NULL);
	converged = 1; // if converge, never use it again.
      }
    }   // for if
    pthread_mutex_unlock(&lock);
  }// for while(1)
  return NULL;
}
