#include "feature_payload.h"
#include "mtp_send.h"

/* file locals */
struct vid_addr_tuple *main_vid_tbl_head = NULL;
//struct vid_addr_tuple *bkp_vid_tbl_head = NULL; // we can maintain backup paths in Main VID Table only, just a thought
struct child_pvid_tuple *cpvid_tbl_head = NULL;
struct local_bcast_tuple *local_bcast_head = NULL;
struct Host_Address_tuple *HAT_head = NULL;
struct control_ports *control_ports_head = NULL;

/*
 *   isChild() - This method checks if the input VID param is child of any VID in Main
 * 		 table or backup vid table.
 *   @input
 *   param1 -   char *       vid
 *
 *   @return
 *   2  - duplicate
 *   1 	- if is a child of one of VID's in main VID Table.
 *   0 	- if VID is parent ID of one of the VID's in the main VID Table.
 *  -1  - if VID is not child of any of the VID's in the main VID Table.
 *
 */

int isChild(char *vid) {

	// For now just checking only the Main VID table.
	if (main_vid_tbl_head != NULL) {
		struct vid_addr_tuple *current = main_vid_tbl_head;

		while (current != NULL){
		  int lenInputVID = strlen(vid);
      int lenCurrentVID = strlen(current->vid_addr);

      // This check is mainly if we get a parent ID, we have eliminate this as the VID is a parent ID.
      if (lenCurrentVID > lenInputVID && strncmp(current->vid_addr, vid, lenInputVID) == 0) {
        lenCurrentVID = lenInputVID;
        return 0;
      }

      // if length is same and are similar then its a duplicate no need to add.
      if ((lenInputVID == lenCurrentVID) && (strncmp(vid, current->vid_addr, lenCurrentVID)==0)) {

        return 2;
      }

			if (strncmp(vid, current->vid_addr, lenCurrentVID)==0) {
				return 1;
			}
			current = current->next;
		}
	}
	return -1;
}


/*
 *   VID Advertisment - This message is sent when a JOIN message is received from non MT Switch
 *   @input
 *   param1 -   uint8_t       payload buffer
 *
 *   @output
 *   payloadLen
 *
 */

// Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS>  <PATH COST> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT>
int  build_VID_ADVT_PAYLOAD(uint8_t *data, char *interface) {
  int payloadLen = 3;
  int numAdvts = 0;
  int egressPort = 0;

  struct vid_addr_tuple *current = main_vid_tbl_head;

  // Port from where VID advt  came.
  int i;
  for(; interface[i]!='\0'; i++) {
    if(interface[i] >= 48 && interface[i] <= 57) {
      egressPort = (egressPort * 10) + (interface[i] - 48);
    }
  }

  while (current != NULL) {
    char vid_addr[VID_ADDR_LEN];

    // <PATH COST> - Taken as '1' for now
    data[payloadLen] = (uint8_t) (current->path_cost + 1);

    // next byte
    payloadLen = payloadLen + 1;

    memset(vid_addr, '\0', VID_ADDR_LEN);
    if (strncmp(interface, current->eth_name, strlen(interface)) == 0) {
      sprintf(vid_addr, "%s", current->vid_addr );
    } else {
      sprintf(vid_addr, "%s.%d", current->vid_addr, egressPort );
    }

    // <VID_ADDR_LEN>
    data[payloadLen] = strlen(vid_addr);

    // next byte
    payloadLen = payloadLen + 1;

    memcpy(&data[payloadLen], vid_addr, strlen(vid_addr));

    payloadLen += strlen(vid_addr);
    current = current->next;
    numAdvts++;

    /* VID Advts should not be more than 3, because we need to advertise only the entries that are in Main VID Table
       from 3 we consider every path as backup path. */
    if (numAdvts >=3 ) {
        break;
    }
  }

  if (numAdvts > 0) {
    // <MSG_TYPE> - Hello Join Message, Type - 3.
    data[0] = (uint8_t) MTP_TYPE_VID_ADVT;

    // <OPERATION>
    data[1] = VID_ADD;

    // <NUMBER_VIDS> - Number of VID's
    data[2] = (uint8_t) numAdvts;
  } else {
    payloadLen = 0;
  }

  // Return the total payload Length.
  return payloadLen;
}

/*
 *   build_JOIN_MSG_PAYLOAD - Join message sent when a non MTP switch wants to be
 *			      be part of MT_Topology. This switch Main VID Table is
 * 			      empty intially.
 *   @input
 *   param1 - 	uint8_t       payload buffer
 *
 *   @output
 *   payloadLen
 *
 */

// Message ordering <MSG_TYPE>
int  build_JOIN_MSG_PAYLOAD(uint8_t *data) {
  int payloadLen = 0;

  // <MSG_TYPE> - Hello Join Message, Type - 1.
  data[payloadLen] = (uint8_t) MTP_TYPE_JOIN_MSG;

  // next byte
  payloadLen = payloadLen + 1;

  // Return the total payload Length.
  return payloadLen;
}

/*
 *   build_PERIODIC_MSG_PAYLOAD - This message is sent every 2 seconds to inform connected
 *                                peers about its perscence.
 *
 *   @input
 *   param1 -   uint8_t           payload buffer
 *
 *   @output
 *   payloadLen
 *
 */

// Message ordering <MSG_TYPE>
int  build_PERIODIC_MSG_PAYLOAD(uint8_t *data) {
  int payloadLen = 0;

  // <MSG_TYPE> - Hello Periodic Message, Type - 2.
  data[payloadLen] = (uint8_t) MTP_TYPE_PERODIC_MSG;

  // next byte
  payloadLen = payloadLen + 1;

  // Return the total payload Length.
  return payloadLen;
}

/*
 *   build_VID_CHANGE_PAYLOAD - This message is sent when ever we miss periodic message for a time of 3 * hello_time.
 *                              Respective VID is deleted and adjacent peers who have VID derived from deleted VID are notified.
 *
 *   @input
 *   param1     -   uint8_t           payload buffer
 *   interface  -   char              interface name
 *   deletedVIDs-   char**            all recently deleted VIDs
 *   numberOfDel-   int               total number of deletions deletedVIDs reference pointing to.
 *
 *   @output
 *   payloadLen
 *
 */

// Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT>
int  build_VID_CHANGE_PAYLOAD(uint8_t *data, char *interface, char **deletedVIDs, int numberOfDeletions) {
  int payloadLen = 3;
  int numAdvts = 0;
  int egressPort = 0;

  // Port from where VID request came.
  int i = 0;
  for(; interface[i]!='\0'; i++) {
    if(interface[i] >= 48 && interface[i] <= 57) {
      egressPort = (egressPort * 10) + (interface[i] - 48);
    }
  }

  i = 0;
  for (; i < numberOfDeletions; i++) {
    char vid_addr[VID_ADDR_LEN];

    memset(vid_addr, '\0', VID_ADDR_LEN);
    //sprintf(vid_addr, "%s.%d", deletedVIDs[i], egressPort ); for now, lets see if don't append egress port
    sprintf(vid_addr, "%s", deletedVIDs[i]);

    // <VID_ADDR_LEN>
    data[payloadLen] = strlen(vid_addr);

    // next byte
    payloadLen = payloadLen + 1;

    memcpy(&data[payloadLen], vid_addr, strlen(vid_addr));
    payloadLen += strlen(vid_addr);
    numAdvts++;
  }

  if (numAdvts > 0) {
    // <MSG_TYPE> - Hello Join Message, Type - 3.
    data[0] = (uint8_t) MTP_TYPE_VID_ADVT;

    // <OPERATION>
    data[1] = VID_DEL;

    // <NUMBER_VIDS> - Number of VID's
    data[2] = (uint8_t) numAdvts;
  } else {
    payloadLen = 0;
  }

  return payloadLen;
}
/*
 *   isMain_VID_Table_Empty -     Check if Main VID Table is empty.
 *
 *   @input
 *   no params
 *
 *   @return
 *   true  	- if Main VID Table is empty.
 *   false 	- if Main VID Table is not null.
 */

// Message ordering <MSG_TYPE>
bool isMain_VID_Table_Empty() {

	if (main_vid_tbl_head) {
		return false;
	}

  return true;
}

/**
 * 		Add into the VID Table, new VID's are added based on the path cost.
 *     		VID Table,		Implemented Using linked list.
 * 		Head Ptr,		*vid_table
 * 		@return
 * 		true 			Successful Addition
 * 		false 			Failure to add/ Already exists.
**/

bool add_entry_LL(struct vid_addr_tuple *node) {
	struct vid_addr_tuple *current = main_vid_tbl_head;
	// If the entry is not already present, we add it.
	if (!find_entry_LL(node)) {
		if (main_vid_tbl_head == NULL) {
				node->membership = 1;
				main_vid_tbl_head = node;
		} else {
			struct vid_addr_tuple *previous = NULL;

			int mship = 0;
			// place in accordance with cost, lowest to highest.
			while(current!=NULL && (current->path_cost < node->path_cost)) {
				previous = current;
				mship = current->membership;
				current = current->next;
			}

			// if new node has lowest cost.
			if (previous == NULL) {
				node->next = main_vid_tbl_head;
				node->membership = 1;
				main_vid_tbl_head = node;
			} else {
				previous->next = node;
				node->next = current;
				node->membership = (mship + 1);
			}

			// Increment the membership IDs of other VID's
			while (current != NULL) {
				current->membership++;
				current = current->next;
			}
		}
		return true;
	}
	return false;
}

/**
 *		Check if the VID entry is already present in the table.
 *      VID Table,		Implemented Using linked list.
 * 		Head Ptr,		*vid_table
 *
 *		@return
 *		true			Element Found.
 *		false			Element Not Found.
**/

bool find_entry_LL(struct vid_addr_tuple *node) {
	if (main_vid_tbl_head != NULL) {
		struct vid_addr_tuple *current = main_vid_tbl_head;
		while (current != NULL) {
			if (strcmp(current->vid_addr, node->vid_addr) == 0) {
				// Update time stamp.
				current->last_updated = time(0);
				return true;
			}
			current = current->next;
		}
	}
	return false;
}

/**
 *		Print VID Table.
 *      	VID Table,		Implemented Using linked list.
 * 		Head Ptr,		*vid_table
 *
 *		@return
 *		void
**/

void print_entries_LL() {
  struct vid_addr_tuple *current;
  int tracker = MAX_MAIN_VID_TBL_PATHS;

  printf("\n#######Main VID Table#########\n");
  printf("MT_VID\t\tEthname\t\tPath Cost\tMembership\tMAC\n");

  for (current = main_vid_tbl_head; current != NULL; current = current->next) {
    if (tracker <= 0) {
      break;
    } else {
      printf("%s\t\t%s\t\t%d\t\t%d\t\t%s\n", current->vid_addr, current->eth_name, current->path_cost, current->membership, ether_ntoa(&current->mac) );
      tracker--;
    }
  }
}

/**
 *              Update timestamp for a MAC address on both VID Table and backup table.
 *     		VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              bool
**/

bool update_hello_time_LL(struct ether_addr *mac) {
  struct vid_addr_tuple *current;
  bool hasUpdates = false;

  for (current = main_vid_tbl_head; current != NULL; current = current->next) {
    if (memcmp(&current->mac, mac, sizeof (struct ether_addr))==0) {
      current->last_updated = time(0);
      hasUpdates = true;
    }
  }
  return hasUpdates;
}

/**
 *              Check for link Failures.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              void
**/

int checkForFailures(char **deletedVIDs) {
  struct vid_addr_tuple *current = main_vid_tbl_head;
  struct vid_addr_tuple *previous = NULL;
  time_t currentTime = time(0);
  int numberOfFailures = 0;

  while (current != NULL) {
    if ((current->last_updated !=-1) &&(currentTime - current->last_updated) > (3 * PERIODIC_HELLO_TIME) ) {
      struct vid_addr_tuple *temp = current;
      deletedVIDs[numberOfFailures] = (char*)calloc(strlen(temp->vid_addr), sizeof(char));
      if (previous == NULL) {
        main_vid_tbl_head	= current->next;
      } else {
        previous->next = current->next;
      }
      strncpy(deletedVIDs[numberOfFailures], temp->vid_addr, strlen(temp->vid_addr));
      current = current->next;
      numberOfFailures++;
      free(temp);
      continue;
    }
    previous = current;
    current = current->next;
  }

  // if failures are there
  if (numberOfFailures > 0) {
    int membership = 1;
    for (current = main_vid_tbl_head; current != NULL; current = current->next) {
      current->membership = membership;
      membership++;
    }
  }
  return numberOfFailures;
}

/**
 *              Delete Entries in the vid_table using vid as a reference.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              true - if deletion is successful.
 *              false - if deletion fails.
**/

bool delete_entry_LL(char *vid_to_delete) {
  struct vid_addr_tuple *current = main_vid_tbl_head;
  struct vid_addr_tuple *previous = NULL;
  bool hasDeletions = false;

  while (current != NULL) {
    if (strncmp(vid_to_delete, current->vid_addr, strlen(vid_to_delete)) == 0) {
      struct vid_addr_tuple *temp = current;

      if (previous == NULL) {
        main_vid_tbl_head = current->next;
      } else {
        previous->next = current->next;
      }

      current = current->next;
      hasDeletions = true;
      free(temp);
      continue;
    }
    previous = current;
    current = current->next;
  }

  // fix any wrong membership values.
  if (hasDeletions) {
    int membership = 1;
    for (current = main_vid_tbl_head; current != NULL; current = current->next) {
      current->membership = membership;
      membership++;
    }
  }
  return hasDeletions;
}


/**
 *              Get Instance of Main VID Table.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              struct main_vid_tbl_head* - reference of main vid table.
**/

struct vid_addr_tuple* getInstance_vid_tbl_LL() {
  return main_vid_tbl_head;
}

/**
 *    Print VID Table.
 *    Backup VID Paths,    Implemented Using linked list, instead of maintaining a seperate table, I am adding Main VIDS and Backup Paths
 *                         in the same table.
 *    Head Ptr,   *vid_table
 *
 *    @return
 *    void
**/

void print_entries_bkp_LL() {
  struct vid_addr_tuple *current;
  int tracker = MAX_MAIN_VID_TBL_PATHS;

  printf("\n#######Backup VID Table#########\n");
  printf("MT_VID\t\tEthname\t\tPath Cost\tMembership\tMAC\n");

  for (current = main_vid_tbl_head; current != NULL; current = current->next) {
    if (tracker <= 0) {
      printf("%s\t\t%s\t\t%d\t\t%d\t\t%s\n", current->vid_addr, current->eth_name, current->path_cost, current->membership, ether_ntoa(&current->mac) );
    } else {
      tracker--;
    }
  }
}

/**
 *    Add into the Child PVID Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *    @return
 *    true      Successful Addition
 *    false     Failure to add/ Already exists.
**/

bool add_entry_cpvid_LL(struct child_pvid_tuple *node) {
  if (cpvid_tbl_head == NULL) {
    cpvid_tbl_head = node;
    return true;
  } else if (update_entry_cpvid_LL(node))  {      // if already, there and there is PVID change.

  } else {
    if (!find_entry_cpvid_LL(node)) {
      node->next = cpvid_tbl_head;
      cpvid_tbl_head = node;
      return true;
    }
  }
  return false;
}

/**
 *    Check if the VID entry is already present in the table.
 *    Child PVID Table,  Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *    @return
 *    true      Element Found.
 *    false     Element Not Found.
**/

bool find_entry_cpvid_LL(struct child_pvid_tuple *node) {
  struct child_pvid_tuple *current = cpvid_tbl_head;

  if (current != NULL) {

    while (current != NULL) {

      if (strcmp(current->vid_addr, node->vid_addr) == 0) {

        return true;
      }

      current = current->next;
    }
  }

  return false;
}

/**
 *    Print Child PVID  Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *
 *    @return
 *    void
**/

void print_entries_cpvid_LL() {
  struct child_pvid_tuple *current;

  printf("\n#######Child PVID Table#########\n");
  printf("Child PVID\t\tPORT\t\t\tMAC\n");

  for (current = cpvid_tbl_head; current != NULL; current = current->next) {
    printf("%s\t\t\t%s\t\t\t%s\n", current->vid_addr, current->child_port, ether_ntoa(&current->mac) );
  }
}

/**
 *    Get instance of child of PVID Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/

struct child_pvid_tuple* getInstance_cpvid_LL() {

  return cpvid_tbl_head;
}

/**
 *    Delete any Child PVID's matching this VID.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *
 *    @input
 *    char * - cpvid to be deleted.
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/

bool delete_entry_cpvid_LL(char *cpvid_to_be_deleted) {
  struct child_pvid_tuple *current = cpvid_tbl_head;
  struct child_pvid_tuple *previous = NULL;
  bool hasDeletions = false;

  while (current != NULL) {
    if (strncmp(cpvid_to_be_deleted, current->vid_addr, strlen(cpvid_to_be_deleted)) == 0) {
      struct child_pvid_tuple *temp = current;

      if (previous == NULL) {
        cpvid_tbl_head = current->next;
      } else {
        previous->next = current->next;
      }

      current = current->next;
      free(temp);
      hasDeletions = true;
      continue;
    }
    previous = current;
    current = current->next;
  }
  return hasDeletions;
}

/**
 *    Delete any Child PVID's matching this VID.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *cpvid_tbl_head
 *
 *    @input
 *    char * - cpvid to be deleted.
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/

bool delete_MACentry_cpvid_LL(struct ether_addr *mac) {
  struct child_pvid_tuple *current = cpvid_tbl_head;
  struct child_pvid_tuple *previous = NULL;
  bool hasDeletions = false;

  while (current != NULL) {
    if (memcmp(mac, &current->mac, sizeof(struct ether_addr)) == 0) {
      struct child_pvid_tuple *temp = current;

      if (previous == NULL) {
        cpvid_tbl_head = current->next;
      } else {
        previous->next = current->next;
      }

      current = current->next;
      free(temp);
      hasDeletions = true;
      continue;
    }
    previous = current;
    current = current->next;
  }
  return hasDeletions;
}

/**
 *              Update timestamp for a MAC address on both CPVID Table and backup table.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *cpvid_tbl_head
 *
 *              @return
 *              void
**/

bool update_hello_time_cpvid_LL(struct ether_addr *mac) {
  struct child_pvid_tuple *current;
  bool isUpdated = false;

  for (current = cpvid_tbl_head; current != NULL; current = current->next) {
    if (memcmp(&current->mac, mac, sizeof (struct ether_addr))==0) {
      current->last_updated = time(0);
      isUpdated = true;
    }
  }
  return isUpdated;
}

/**
 *              Update timestamp for a MAC address on both CPVID Table.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *cpvid_tbl_head
 *
 *              @return
 *              void
**/

bool update_entry_cpvid_LL(struct child_pvid_tuple *node) {
  struct child_pvid_tuple *current;

  for (current = cpvid_tbl_head; current != NULL; current = current->next) {
    if (memcmp(&current->mac, &node->mac, sizeof(struct ether_addr)) == 0) {
      memset(current->vid_addr, '\0', VID_ADDR_LEN);
      strncpy(current->vid_addr, node->vid_addr, strlen(node->vid_addr));
      return true;
    }
  }
  return false;
}

/**
 *              Check for link Failures.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *cpvid_tbl_head
 *
 *              @return
 *              void
**/

bool checkForFailuresCPVID() {
  struct child_pvid_tuple *current = cpvid_tbl_head;
  struct child_pvid_tuple *previous = NULL;
  time_t currentTime = time(0);
  bool hasDeletions = false;

  while (current != NULL) {
    if ((current->last_updated !=-1) &&(currentTime - current->last_updated) > (3 * PERIODIC_HELLO_TIME) ) {
      struct child_pvid_tuple *temp = current;
      if (previous == NULL) {
        cpvid_tbl_head = current->next;
      } else {
        previous->next = current->next;
      }
      current = current->next;
      free(temp);
      hasDeletions = true;
      continue;
    }
    previous = current;
    current = current->next;
  }
  return hasDeletions;
}

/**
 *    Add into the local host broadcast Table.
 *    Local Host broadcast Table, Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true      Successful Addition
 *    false     Failure to add/ Already exists.
**/

bool add_entry_lbcast_LL(struct local_bcast_tuple *node) {
  if (local_bcast_head == NULL) {
    local_bcast_head = node;
  } else {
    if (!find_entry_lbcast_LL(node)) {
      node->next = local_bcast_head;
      local_bcast_head = node;
    }
  }
}



bool find_entry_lbcast_LL(struct local_bcast_tuple *node) {
  struct local_bcast_tuple *current = local_bcast_head;

  if (current != NULL) {

    while (current != NULL) {

      if (strcmp(current->eth_name, node->eth_name) == 0) {
        return true;
      }

      current = current->next;
    }
  }

  return false;
}

/**
 *    Print local host broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    void
**/

void print_entries_lbcast_LL() {
  struct local_bcast_tuple *current;


  printf("\n#######Local Host Broadcast Table#########\n");
  printf("PORT\n");

  for (current = local_bcast_head; current != NULL; current = current->next) {
    printf("%s\n", current->eth_name);
  }
}

/**
 *    Delete a port from local broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true  - if deletion successful.
 *    false - if deletion not successful.
**/

bool delete_entry_lbcast_LL(char *port) {
  struct local_bcast_tuple *current = local_bcast_head;
  struct local_bcast_tuple *previous = NULL;
  bool isPortDeleted = false;

  //printf("%s deleted from local host table value of \n",current->eth_name);
  while (current != NULL) {
    if (strcmp(current->eth_name, port) == 0) {
      if (current == local_bcast_head) {
        local_bcast_head = current->next;
        free(current);
        current = local_bcast_head;
        previous = NULL;
        continue;
      } else {
        struct local_bcast_tuple *temp = current;
        current = current->next;
        previous->next = current;
        free(temp);
        continue;
      }
      isPortDeleted = true;
    }
    previous = current;
    current = current->next;
  }
  //print_entries_lbcast_LL();
  return isPortDeleted;
}

/**
 *    Return instance of local broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    struct local_bcast_tuple* - Returns reference to local host broadcast table.
**/

struct local_bcast_tuple* getInstance_lbcast_LL() {
  return local_bcast_head;
}

/* NS adds code for populating Host Address Table */
bool add_entry_HAT_LL(struct Host_Address_tuple *HAT) {
	printf("\nEntered adding section\n");
  if (HAT_head == NULL) {
    HAT_head = HAT;  // update local variable with the passed variable - first time this will be true
  } else {
    if (!find_entry_HAT_LL(HAT)) {  // second time onwards this will be true
      //printf(" In ADD HAT enrty \n  " );
      HAT->next = HAT_head;
      HAT_head = HAT;  // what is happening here - the entry is being added
    }
  }
  printf("\nCompleted HAA entry section\n");
}

bool find_entry_HAT_LL(struct Host_Address_tuple *node) {
  struct Host_Address_tuple *current = HAT_head;
  uint8_t mac_addr [6];
  uint8_t byte_number;

  if (current != NULL) {
    //printf(" In Locating HAT enrty - first step\n  " );
    while (current != NULL) {
      //printf(" In Locating HAT enrty - second step\n  " );
      for (byte_number=0; byte_number< 6; byte_number++)
      {
        printf ("byte number =%d, in table mac octet = %d, in node mac octet = %d   \n", byte_number, current->mac.ether_addr_octet[byte_number], node->mac.ether_addr_octet[byte_number]);
        if (current->mac.ether_addr_octet[byte_number] != node->mac.ether_addr_octet[byte_number])
          break;
      }
      if (byte_number ==6) {

      printf(" \nIn Locating HAT entry found a match. Nothing will be added.\n  " );
        return true;
      }
      current = current->next;
    }
  }
  return false;
}

void print_entries_HAT_LL() {
  struct Host_Address_tuple *current;
  printf("\n###################   Host Address Table   ###############\n");

  for (current = HAT_head; current != NULL; current = current->next) {
    printf("port name \t\t cost \t\t seq num \t\t mac\t\t local \t\t switch-id\n");
	printf("%s\t\t\t%d\t\t%d\t\t%s\t\t%d\t\t", current->eth_name, current->path_cost,  current->sequence_number, ether_ntoa(&current->mac), current->local);
	printf("%s\n",ether_ntoa((struct ether_addr*)&current->switch_id->ether_addr_octet));
 }
  printf("\n###################   End Host Address Table   ###############\n");
}

int build_HAAdvt_message(uint8_t *data, struct ether_addr mac, uint8_t cost, uint8_t sequence_number, struct ether_addr *switch_id) {
	int payloadLen = 1; //to store message type

	//struct Host_Address_tuple *current = HAT_head;

		data[payloadLen] = (uint8_t) (cost);
		payloadLen = payloadLen +1;
        data[payloadLen] = (uint8_t) (sequence_number);
		payloadLen = payloadLen +1;
		data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[0];
		data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[1];
		data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[2];
		data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[3];
		data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[4];
		data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[5];
		data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[0];
		data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[1];
		data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[2];
		data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[3];
		data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[4];
		data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[5];
		//printf("%s\n",ether_ntoa((struct ether_addr*)&switch_id->ether_addr_octet));
		data[0] = MTP_HAAdvt_TYPE;
	return payloadLen;
}
/*
struct  ether_header {
  u_char  ether_dhost[6];
  u_char  ether_shost[6];
  u_short ether_type;
};
struct ether_header *eheader = NULL;

*/

void print_HAAdvt_message_content(uint8_t *rx_buffer)
{
  uint8_t mac_addr [6],temp [6];

  mac_addr[0] = rx_buffer[17];
  mac_addr[1] = rx_buffer[18];
  mac_addr[2] = rx_buffer[19];
  mac_addr[3] = rx_buffer[20];
  mac_addr[4] = rx_buffer[21];
  mac_addr[5] = rx_buffer[22];
  temp[0] = rx_buffer[23];
  temp[1] = rx_buffer[24];
  temp[2] = rx_buffer[25];
  temp[3] = rx_buffer[26];
  temp[4] = rx_buffer[27];
  temp[5] = rx_buffer[28];

printf("\nCost is %d, Sequqnce number is %d, MAC address is %s \n", (uint8_t) rx_buffer[15], (uint8_t) rx_buffer[16], ether_ntoa((struct ether_addr *) mac_addr));
printf("Switch Id is %s\n", ether_ntoa((struct ether_addr *) temp));
}

/**
*     NS - Create a table that stores the control ports
 *    Add into the control ports Table.
 *    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true      Successful Addition
 *    false     Failure to add/ Already exists.
**/
struct control_ports* getInstance_control_LL() {
  return control_ports_head;
}

bool add_entry_control_table(struct control_ports *node) {
  if (control_ports_head == NULL) {
    control_ports_head = node;
  } else {
    if (!find_entry_control_table(node)) {
      node->next = control_ports_head;
      control_ports_head = node;
      print_entries_control_table ();
    }
  }
}

/**
 *    Find entries in the local host broadcast Table.
 *    is being used for finding entries in control ports table also
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true      If element is found.
 *    false     If element is not found.
**/
 bool find_entry_control_table(struct control_ports  *node) {
  struct control_ports  *current = control_ports_head;

  if (current != NULL) {

    while (current != NULL) {

      if (strcmp(current->eth_name, node->eth_name) == 0) {
        return true;
      }

      current = current->next;
    }
  }

  return false;
}

/**
 *    Print Control Ports Table.
 *    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    void
**/

void print_entries_control_table() {
  struct control_ports *current;


  printf("\n#######Control ports Table#########\n");

  for (current = control_ports_head; current != NULL; current = current->next) {
    printf("%s\n", current->eth_name);
  }
}

struct ether_addr* get_switchid() //created by Guru and Rajesh
{
	struct control_ports *current;
	int fd,i=0,j,k,flag=0;
    struct ifreq ifr;
    char *iface;
    struct ether_addr *temp_mac =(struct ether_addr *) calloc (1, sizeof (struct ether_addr));

	for (current = control_ports_head; (current != NULL || flag!=1); current = current->next){
		if(current==NULL && flag ==0){
			current = (struct control_ports *) local_bcast_head;
			printf("\n%s\n",current->eth_name);
			flag=1;
		}
		struct ether_addr *mac = (struct ether_addr *) calloc (1, sizeof (struct ether_addr));
		iface=current->eth_name;
		//printf("\nFlag:%d\t%s\n",flag,current->eth_name);
		print_entries_lbcast_LL();

	fd = socket(AF_INET, SOCK_DGRAM, 0);

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name , iface , IFNAMSIZ-1);

    ioctl(fd, SIOCGIFHWADDR, &ifr);

    close(fd);

    mac = (struct ether_addr *)ifr.ifr_hwaddr.sa_data;
	if(i==0){
    for(k=0;k<6;k++){
	temp_mac->ether_addr_octet[k]=mac->ether_addr_octet[k];
	}
	i=1;
	}
	else{
		for(j=0;j!=6;j++){
				//printf("Old Octet:%d   New Octet:%d\n",temp_mac->ether_addr_octet[j],mac->ether_addr_octet[j]);
				if((mac->ether_addr_octet[j])>(temp_mac->ether_addr_octet[j])){
				break;
				}
				else if((mac->ether_addr_octet[j])<(temp_mac->ether_addr_octet[j])){
					for(k=0;k<6;k++){
				temp_mac->ether_addr_octet[k]=mac->ether_addr_octet[k];
				}
				break;
				}

			}
			//printf("Iterated Mac : %.2x:%.2x:%.2x:%.2x:%.2x:%.2x \n" , temp_mac->ether_addr_octet[0], temp_mac->ether_addr_octet[1], temp_mac->ether_addr_octet[2], temp_mac->ether_addr_octet[3], temp_mac->ether_addr_octet[4], temp_mac->ether_addr_octet[5]);
		}


    //display mac address
    //printf("Confirmed Mac : %.2x:%.2x:%.2x:%.2x:%.2x:%.2x \n" , temp_mac->ether_addr_octet[0], temp_mac->ether_addr_octet[1], temp_mac->ether_addr_octet[2], temp_mac->ether_addr_octet[3], temp_mac->ether_addr_octet[4], temp_mac->ether_addr_octet[5]);
	}
	return temp_mac;

}
