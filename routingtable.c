#include "ne.h"
#include "router.h"
#include <stdbool.h>
#include <string.h>


/* ----- GLOBAL VARIABLES ----- */
struct route_entry routingTable[MAX_ROUTERS];
int NumRoutes = 0;

////////////////////////////////////////////////////////////////
void InitRoutingTbl (struct pkt_INIT_RESPONSE *InitResponse, int myID){
	/* ----- YOUR CODE HERE ----- */
        //init
	int i;
	//construct initail routing table
	for (i = 0;i < InitResponse->no_nbr; i++){
	  routingTable[i].dest_id = InitResponse->nbrcost[i].nbr;
	  routingTable[i].next_hop = InitResponse->nbrcost[i].nbr;
	  routingTable[i].cost = InitResponse->nbrcost[i].cost;
	  routingTable[i].path_len = 2;
	  routingTable[i].path[0] = myID;
	  routingTable[i].path[1] = InitResponse->nbrcost[i].nbr;
	  NumRoutes += 1;
	}
	NumRoutes += 1;
	routingTable[i].dest_id = myID;
	routingTable[i].next_hop = myID;
	routingTable[i].cost = 0;
	routingTable[i].path_len = 1;
	routingTable[i].path[0] = myID;
	return;
}


////////////////////////////////////////////////////////////////
int UpdateRoutes(struct pkt_RT_UPDATE *RecvdUpdatePacket, int costToNbr, int myID){
	/* ----- YOUR CODE HERE ----- */
  struct route_entry update_entry;
  struct route_entry cur_entry;
  bool found_entry = 0;
  bool changed = 0;
  int i, j, k;
  for (i = 0; i < RecvdUpdatePacket->no_routes; i++){
    found_entry = 0;
    update_entry = RecvdUpdatePacket->route[i];
    int new_cost = costToNbr + update_entry.cost;
    for (j = 0; j < NumRoutes; j++){
      cur_entry = routingTable[j];
      //found the entry
      if (routingTable[j].dest_id == update_entry.dest_id){
	found_entry = 1;
	if (new_cost < routingTable[j].cost && update_entry.next_hop != myID){ //force update prevent loop and find better path
	  for (k = 0; k < routingTable[j].path_len; k++){
	    routingTable[j].path[k + 1] = update_entry.path[k];
	  }
	  routingTable[j].next_hop = RecvdUpdatePacket->sender_id;
	  routingTable[j].cost = new_cost;
	  routingTable[j].path_len = update_entry.path_len;
	  changed = 1;
	}
	else if (RecvdUpdatePacket->sender_id == routingTable[j].next_hop){//force update
	  routingTable[j].cost = new_cost;
	  changed = 1;
	}
	//break;
	j = NumRoutes;
      }
    }
    //in case entry is not found
    if (found_entry == 0){
      routingTable[j].next_hop = RecvdUpdatePacket->sender_id;
      routingTable[j].cost = new_cost;
      routingTable[j].dest_id = update_entry.dest_id;
      routingTable[j].path_len = update_entry.path_len + 1;
      printf("\n%ld %d\n",sizeof(routingTable[j].path)/4,update_entry.path_len);
      for (k = 0; k < update_entry.path_len; k++){
        routingTable[j].path[k + 1] = update_entry.path[k];
      }
      changed = 1;
      NumRoutes += 1;
      //found_entry = 1;
    }
  }
  return changed;
}
	      /*if (new_cost < routingTable[j].cost && update_entry.next_hop != myID){ //force update prevent loop and find better path
		routingTable[j].next_hop = RecvdUpdatePacket->sender_id;
		routingTable[j].cost = new_cost;
		changed = 1;
	      }
	      else if (RecvdUpdatePacket->sender_id == routingTable[j].next_hop){//force update
	      routingTable[j].cost = new_cost;
	      changed = 1;
	      }*/
////////////////////////////////////////////////////////////////
void ConvertTabletoPkt(struct pkt_RT_UPDATE *UpdatePacketToSend, int myID){
	/* ----- YOUR CODE HERE ----- */
        UpdatePacketToSend->sender_id = myID;
	UpdatePacketToSend->no_routes = NumRoutes;
	UpdatePacketToSend->dest_id = routingTable[myID].next_hop;
	int i;
	//int j;
	for (i = 0; i < NumRoutes; i++){
	  UpdatePacketToSend->route[i] = routingTable[i];
	}
	return;
}


////////////////////////////////////////////////////////////////
//It is highly recommended that you do not change this function!
void PrintRoutes (FILE* Logfile, int myID){
	/* ----- PRINT ALL ROUTES TO LOG FILE ----- */
	int i;
	int j;
	for(i = 0; i < NumRoutes; i++){
		fprintf(Logfile, "<R%d -> R%d> Path: R%d", myID, routingTable[i].dest_id, myID);

		/* ----- PRINT PATH VECTOR ----- */
		for(j = 1; j < routingTable[i].path_len; j++){
			fprintf(Logfile, " -> R%d", routingTable[i].path[j]);	
		}
		fprintf(Logfile, ", Cost: %d\n", routingTable[i].cost);
	}
	fprintf(Logfile, "\n");
	fflush(Logfile);
}


////////////////////////////////////////////////////////////////
void UninstallRoutesOnNbrDeath(int DeadNbr){
	/* ----- YOUR CODE HERE ----- */
        int i = 0 ;
        for (i = 0; i < NumRoutes; i++){
	  if (routingTable[i].next_hop == DeadNbr){
	    routingTable[i].cost = INFINITY;
	  }
        }
	return;
}
