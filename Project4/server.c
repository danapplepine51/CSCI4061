/**
 * server.c
 *
 * by: Daniel Song, Isaac Stein
 *
 * last modified: 2018 Apr. 29
 *
 * This file details client functionality of our program.
 * This file also contains many of the functions used for send, receive and
    modify poll information or structure from or to clients.
 */

/********************************************************************
 * Includes
 *******************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server_util.h"

#define NUM_ARGS 2
#define MAX_CONNECTION 1000
#define PDAG (*dag)

/********************************************************************
 * Structures
 *******************************************************************/
enum poll_state {
	NEW,
	OPEN,
	CLOSED
};

struct request{
  char *code;
  char *region;
  char *data;
};

struct response{
  char *code;
  char *data;
};

struct candidates{
  int num_candidates;
  int max_candidates;
  char **names;
  int *votes;
};

typedef struct dag{
    struct dag *parent;
    char *name;
    int num_child;
    int index;
    enum poll_state state;
    struct dag **child;
    struct candidates *candidates;
    pthread_mutex_t mutex;
}dag_t, *dag_pt;

typedef struct threadArg{
	char *client_ip;
	int client_port;
	int client_fd;
}threadArg_t, *threadArg_pt;

/********************************************************************
 * Globals
 *******************************************************************/
struct dag *dag;

/********************************************************************
 * Directory Candidates Structure Associated Functions
 *******************************************************************/
/**
 * @brief Frees a candidate structure and all of its data.
 *
 * @param The candidate structure to free.
 */
void FreeCandidates(struct candidates *d) {
  for(int i=0 ; i<d->num_candidates ; i++) free(d->names[i]);
  free(d->names);
  free(d->votes);
  free(d);
}

/**
 * @brief Copys the contents of a candidate array into another.
 *
 * @param A candidate array to copy.
 *
 * @return A new candidate array.
 */
struct candidates *CandCopy(struct candidates *d) {
  struct candidates *dc = malloc(sizeof(struct candidates));
  dc->num_candidates = d->num_candidates;
  dc->max_candidates = d->max_candidates;
  dc->names = malloc(dc->max_candidates*sizeof(char *));
  dc->votes = malloc(dc->max_candidates*sizeof(int));
  for (int i=0; i<dc->num_candidates; i++) {
    dc->names[i] = Concat(d->names[i], "");
    dc->votes[i] = d->votes[i];
  }
  return dc;
}

/**
 * @breif Returns a string which details the entire structure of the candidates.
 *
 * @param The candidates structure to write.
 *
 * @return The details of the structure.
 */
char *GetCandidatesString(struct candidates *d) {
  char *name_string, *vote_string, *full_string, *output=NULL, *new_output;
  if (d == NULL || d->num_candidates == 0) {
    output = malloc(1 * sizeof(char));
    output[0] = '\0';
  }
  for (int i=0 ; i<d->num_candidates ; i++) {
    name_string = Concat(d->names[i],":");
    vote_string = Concat(itoa(d->votes[i]), "\n");
    full_string = Concat(name_string, vote_string);
    new_output = Concat(output, full_string);
    if (output!=NULL) free(output);
    output = new_output;
    free(name_string);
    free(vote_string);
    free(full_string);
  }
  return output;
}

/**
 * @brief Returns a new candidate structure.
 *
 * @return A new candidate structure with zeroed out counters.
 */
struct candidates *NewCandidates() {
  struct candidates *d = (struct candidates *)malloc(sizeof(struct candidates));
  d->num_candidates = 0;
  d->max_candidates = 0;
  return d;
}

/**
 * @brief Lengthens the names and votes array within the candidates array.
 *
 * @param The candidate structure to work on.
 */
void LengthenCandidates(struct candidates *d) {
  if (d->num_candidates < d->max_candidates) return;

  char **new_names = (char **)malloc(sizeof(char *) * (d->max_candidates*2+1));
  int *new_votes = (int *)malloc(sizeof(int) * (d->max_candidates*2+1));

  for (int i=0 ; i<d->max_candidates ; i++) {
    new_names[i] = d->names[i];
    new_votes[i] = d->votes[i];
  }

  //if (d->names != NULL) free(d->names);
  //if (d->votes != NULL) free(d->votes);

  d->names = new_names;
  d->votes = new_votes;
  d->max_candidates = d->max_candidates*2+1;
}

/**
 * @brief Adds an amount of votes to a candidate.
 *
 * @param The candidates structure to work on.
 * @param The name of the candidate to add the votes to.
 * @param The number of votes to add.
 */
void AddVotes(struct candidates *d, char *name, int num_votes) {
  int i;
  for(i=0 ; i<d->num_candidates ; i++) {
    if (!strcmp(name,d->names[i])) {
      d->votes[i] += num_votes;
      if (d->votes[i] < 0) d->votes[i] = 0;
      return;
    }
  }
  if (i == d->max_candidates) LengthenCandidates(d);

  d->names[i] = malloc(sizeof(char) * strlen(name));
  strcpy(d->names[i], name);
  d->votes[d->num_candidates++] = (num_votes >= 0) ? num_votes : 0;
}

/**
 * @brief Adds a vote to a candidate.
 *
 * @param The candidates structure to work on.
 * @param The name of the candidate to add the vote to.
 */
void AddVote(struct candidates *d, char *name) { AddVotes(d, name, 1); }

void AddAllVotes(struct candidates *dest, struct candidates *src) {
  for (int i=0; i<src->num_candidates; i++) {
    AddVotes(dest, src->names[i], src->votes[i]);
  }
}

void SubtractAllVotes(struct candidates *dest, struct candidates *src) {
  for (int i=0; i<src->num_candidates; i++) {
    AddVotes(dest, src->names[i], -src->votes[i]);
  }
}

/********************************************************************
 * DAG Functions
 *******************************************************************/
/**
 * @brief Makes a new DAG node.
 *
 * @return A new DAG node.
 */
dag_pt new_dag() {
    dag_pt tmp = (dag_pt)malloc(sizeof(dag_t));
    tmp->parent = NULL;
    tmp->name = NULL;
    tmp->num_child = -1;
    tmp->index = -1;
    tmp->child = NULL;
    tmp->state = NEW;
    tmp->candidates = NewCandidates();
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutex_init(&tmp->mutex, &mutex_attr);
    return tmp;
}

/**
 * @brief Gets the specific DAG with name.
 *
 * @param DAG structure to search.
 * @param Name to find.
 *
 * @return The DAG with a matching name
 */
struct dag *get_dag(dag_pt dag, char *name) {
    // The parameters are invalid.
    if(dag == NULL || name == NULL) return NULL;

    // The dag was ill-formatted and does not have a name.
    if(dag->name == NULL) return NULL;

    // The dag is a match
    if(!strcmp(dag->name, name)) return dag;
    // Check every child in order to see if they have the requested name.
    // Assumes that the list of children does not contail NULL entries
    //   between actual children.
    struct dag *try_dag = NULL;

    for(int i=0; i<dag->num_child && dag->child[i]!=NULL; i++)
      if ((try_dag = get_dag(dag->child[i], name)) != NULL) return try_dag;

    return try_dag;
}

/**
 * @brief Build DAG structure.
 *
 * @param DAG structure to build.
 * @param Output of tokenized string.
 * @param Token of tokenized string.
 *
 */
void build_dag(dag_pt *dag, char **output, int token, const char *output_path) {
    dag_pt input_dag = PDAG;
    dag_pt find = NULL;

    if(dag == NULL || output == NULL || token == 0) {
        fprintf(stderr, "Error\n"); // What is this error?
    } else {
        for(int i =0 ; i< token ;i ++) {
            char *node = output[i];
            if(i == 0) { /////////parent
                if(input_dag->name == NULL) { /////parent with no dag
                    input_dag->name = malloc(sizeof(char) * strlen(node));
                    strcpy(input_dag->name,node);
                } else { /////parent with initialized dag
		  if((find = get_dag(PDAG, node)) != NULL) {
                    input_dag = find;
		  } else {
		   fprintf(stderr, "Malformed DAG parent region  not defined\n");
	 	   exit(1);
		  }
                }
                input_dag->num_child = token - 1;
                find = NULL;
            } else { ///////child
                if(input_dag->child == NULL) {
                    input_dag->child = (dag_pt*)malloc(sizeof(dag_pt)*token);
                }
                input_dag->child[i-1] = new_dag();
                input_dag->child[i-1]->name = malloc(sizeof(char) * strlen(node));
                strcpy(input_dag->child[i-1]->name,node);
                input_dag->child[i-1]->parent = input_dag;
            }
        }
    }
}
/**
 * @brief Print DAG structure.
 *
 * @param DAG structure to print.
 *
 */
void print_dag(dag_pt dag){
    if(dag == NULL){
        return;
    }
    else{
        if(dag->parent == NULL){
            printf("Parent:root ");
        }
        else{
            printf("Parent:%s ", dag->parent->name);
        }
        printf("Name:%s (%d) ",dag->name, dag->num_child);
        for(int i =0; i < dag->num_child; i++){
            printf("Child %d:%s ",i, dag->child[i]->name);
        }
        printf("\n");
        for(int i =0; i < dag->num_child; i++){
            print_dag(dag->child[i]);
        }
    }
}

/**
 * @brief Function to free DAG structure.
 *
 * @param DAG structure to free.
 */
void free_dag(dag_pt dag){
    if(dag == NULL) return;

	  dag_pt tmp = dag;
    for(int i =0; i < dag->num_child; i++) {
        free_dag(dag->child[i]);
    }
    //printf("Free %s : ",tmp->name);
    //printf("Free child_pt\n");
    free(tmp->child);
    free(tmp);
}

/**
 * @brief Adds a new child onto an existing DAG.
 *
 * @param The DAG to add the child to.
 * @param The child name.
 */
void AddChildDag(struct dag *dag, char *name) {
  struct dag *child_dag = new_dag();
  child_dag->name = Concat(name, "");
  child_dag->parent = dag;

  dag->num_child++;
  if (dag->num_child == 0) dag->num_child++;
  struct dag **children = (struct dag **)malloc(sizeof(struct dag *) * (dag->num_child));
  for (int i=0 ; i<dag->num_child-1 ; i++) {
    children[i] = dag->child[i];
  }
  children[dag->num_child-1] = child_dag;
  free(dag->child);
  dag->child = children;
  printf("%s to %s\n",dag->name, dag->child[0]->name);
}

/********************************************************************
 * String Functions
 *******************************************************************/

/**
 * @brief Adds a set of votes to every node in a DAG going up.
 *
 * @param The dag to add the votes to.
 * @param The candidate structure to add.
 */
void CascadeAdds(struct dag *dag, struct candidates *d) {
  if (dag == NULL || d == NULL) return;
  pthread_mutex_lock(&dag->mutex);
  AddAllVotes(dag->candidates, d);
  pthread_mutex_unlock(&dag->mutex);
  CascadeAdds(dag->parent, d);
}

/**
 * @brief Subtracts a set of votes to every node in a DAG going up.
 *
 * @param The dag to subtract the votes from.
 * @param The candidate structure to subtract.
 */
void CascadeSubtracts(struct dag *dag, struct candidates *d) {
  if (dag == NULL || d == NULL) return;
  pthread_mutex_lock(&dag->mutex);
  SubtractAllVotes(dag->candidates, d);
  pthread_mutex_unlock(&dag->mutex);
  CascadeSubtracts(dag->parent, d);
}

/**
 * @brief Gets the name of the winner in the dag.
 *
 * @return The name of the candidate with the most votes.
 */
char *GetWinnerString() {
  if (dag->candidates->num_candidates == 0) return Concat("No Votes.", "");
  int top_vote = -1;
  int top_spot = -1;
  for (int i=0; i<dag->candidates->num_candidates; i++) {
    if (dag->candidates->votes[i] > top_vote) {
      top_vote = dag->candidates->votes[i];
      top_spot = i;
    }
  }
  return Concat("Winner:", dag->candidates->names[top_spot]);
}

/**
 * @brief Processes an return_winner command.
 *
 * @brief The request detailing the command.
 * @brief The response of the successfullness.
 */
void Return_Winner(struct request *request, struct response *response) {
  if (dag->state != CLOSED) {
    response->code = Concat("RO", "");
    response->data = Concat(dag->name, "");
  } else {
    response->code = Concat("SC", "");
    pthread_mutex_lock(&dag->mutex);
    response->data = GetWinnerString();
    pthread_mutex_unlock(&dag->mutex);
 }
}

/**
 * @brief Converts a candidate structure into a character array representation.
 *
 * @brief The structure representing the votes.
 * @return The character array representation.
 */
char *CandidatesString(struct candidates *cand) {
  if (cand->num_candidates == 0) return Concat("No votes.", "");
  char *data = NULL;
  char *old_data = NULL;
  char *name = NULL;
  char *votes = NULL;
  char *temp = NULL;
  for (int i=0 ; i<cand->num_candidates; i++) {
    old_data = data;
    name = Concat(cand->names[i], ":");
    votes = (i == cand->num_candidates-1) ?
      Concat((itoa(cand->votes[i])), "") :
      Concat((itoa(cand->votes[i])), ",");
    temp = Concat(name, votes);
    data = Concat(data, temp);
    if (old_data != NULL) free(old_data);
    free(name);
    free(votes);
    free(temp);
  }
  return data;
}

/**
 * @brief Processes an count_votes command.
 *
 * @brief The request detailing the command.
 * @brief The response of the successfullness.
 */
void Count_Votes(struct request *request, struct response *response) {
  struct dag *mydag = get_dag(dag, request->region);
  if (mydag == NULL) {
    response->code = Concat("NR", "");
    response->data = Concat(request->region, "");
    return;
  }
  response->code = Concat("SC", "");
  pthread_mutex_lock(&dag->mutex);
  response->data = CandidatesString(mydag->candidates);
  pthread_mutex_unlock(&dag->mutex);
}

/**
 * @brief Opens the polls in the region if applicable.
 *
 * @brief The region to open the polls in.
 */
void Open_Polls_aux(struct dag *dag) {
  if (dag == NULL) return;
  pthread_mutex_lock(&dag->mutex);
  if (dag->state == NEW) dag->state = OPEN;
  pthread_mutex_unlock(&dag->mutex);
  for (int i=0 ; i<(dag->num_child ) ; i++) {
    Open_Polls_aux(dag->child[i]);
  }
}

/**
 * @brief Processes an open_polls command.
 *
 * @brief The request detailing the command.
 * @brief The response of the successfullness.
 */
void Open_Polls(struct request *request, struct response *response) {
  struct dag *mydag = get_dag(dag, request->region);
  if (mydag == NULL) {
    response->code = Concat("NR", "");
    response->data = Concat(request->region, "");
  } else if (mydag->state == OPEN) {
    response->code = Concat("PF", "");
    response->data = Concat(request->region, " open");
  } else if (mydag->state == CLOSED) {
    response->code = Concat("RR", "");
    response->data = Concat(request->region, "");
  } else {
    Open_Polls_aux(mydag);
    response->code = Concat("SC", "");
    response->data = Concat("", "");
  }
}

/**
 * @brief Closes the polls in the region if applicable.
 *
 * @brief The region to close the polls in.
 */
void Close_Polls_aux(struct dag *dag) {
  if (dag == NULL) return;
  pthread_mutex_lock(&dag->mutex);
  if (dag->state == OPEN) dag->state = CLOSED;
  pthread_mutex_unlock(&dag->mutex);
  for (int i=0 ; i<(dag->num_child) ; i++) {
    Close_Polls_aux(dag->child[i]);
  }
}

/**
 * @brief Processes an close_polls command.
 *
 * @brief The request detailing the command.
 * @brief The response of the successfullness.
 */
void Close_Polls(struct request *request, struct response *response) {
  struct dag *mydag = get_dag(dag, request->region);
  if (mydag == NULL) {
    response->code = Concat("NR", "");
    response->data = Concat(request->region, "");
  } else if (mydag->state == CLOSED) {
    response->code = Concat("PF", "");
    response->data = Concat(request->region, " closed");
  } else if (mydag->state == NEW) {
    response->code = Concat("PF", "");
    response->data = Concat(request->region, " unopened");
  } else {
    Close_Polls_aux(mydag);
    response->code = Concat("SC", "");
    response->data = Concat("", "");
  }
}

/**
 * @brief Converts a character string of votes to an appropriate candidate structure representation.
 *
 * @brief The votes to be added.
 * @return The candidates array storing the votes.
 */
struct candidates *CreateCandidates(char *data) {
  struct candidates *d = NewCandidates();
  if (strlen(data) == 0) return d;
  char *name;
  char *tdata = Concat(data, "");
  int num;
  int start_index = 0;
  int middle_index = 0;
  int end_index = 0;
  int ended = 0;
  while (!ended) {
    if (tdata[middle_index] == ':' && (tdata[end_index] == ',' || tdata[end_index] == '\0')) {
      if (tdata[end_index] == '\0') ended = 1;
      if (middle_index <= start_index) {
	fprintf(stderr,"Error: strage vote data string %s\n", data);
      }
      tdata[middle_index++] = '\0';
      tdata[end_index++] = '\0';
      name = Concat(tdata + start_index, "");
      num = atoi(tdata + middle_index);
      AddVotes(d, name, num);
      free(name);
      start_index = end_index;
    } else if (tdata[end_index] == ':') {
      middle_index = end_index++;
    } else {
      end_index++;
    }
  }
  return d;
}

/**
 * @brief Processes an add_votes command.
 *
 * @brief The request detailing the command.
 * @brief The response of the successfullness.
 */
void Add_Votes(struct request *request, struct response *response) {
  struct dag *mydag = get_dag(dag, request->region);
  if (mydag == NULL) {
    response->code = Concat("NR", "");
    response->data = Concat(request->region, "");
  } else if (mydag->num_child >= 0) { // Dag does not actually store the number of children
    response->code = Concat("NL", "");
    response->data = Concat(request->region, "");
  } else if (mydag->state != OPEN) {
    response->code = Concat("RC", "");
    response->data = Concat(request->region, "");
  } else {
    response->code = Concat("SC", "");
    response->data = Concat("", "");
    struct candidates *vote_change = CreateCandidates(request->data);
    CascadeAdds(mydag, vote_change);
    free(vote_change);
  }
}

/**
 * @brief Checks that a set of votes may be removed from a dag without error.
 *        If they can, the votes are then subtracted
 *
 * @brief The response of the successfullness.
 * @brief The base DAG to remove the votes from.
 * @brief The set of votes to subtract.
 */
void TrySubtraction(struct response *response, struct dag *dag, struct candidates *vote_change) {
  response->data = Concat("", "");
  char *temp;
  int valid;
  int first_error = 1;
  for (int c=0 ; c<vote_change->num_candidates ; c++) {
    valid = 0;
    for(int i=0 ; i<dag->candidates->num_candidates ; i++) {
      if (!strcmp(vote_change->names[c], dag->candidates->names[i]) &&
          dag->candidates->votes[i] >= vote_change->votes[c]) {
        valid = 1;
      }
    }
    if (!valid) {
      if (!first_error) {
        temp = response->data;
        response->data = Concat(response->data, ",");
        free(temp);
      } else {
        first_error = 0;
      }
      temp = response->data;
      response->data = Concat(response->data, vote_change->names[c]);
      free(temp);
    }
  }

  if (first_error) {
    response->code = Concat("SC", "");
    CascadeSubtracts(dag, vote_change);
  } else {
    response->code = Concat("IS", "");
  }
}

/**
 * @brief Processes a remove_votes command.
 *
 * @brief The request detailing the command.
 * @brief The response of the successfullness.
 */
void Remove_Votes(struct request *request, struct response *response) {
  struct dag *mydag = get_dag(dag, request->region);
  if (mydag == NULL) {
    response->code = Concat("NR", "");
    response->data = Concat(request->region, "");
  } else if (mydag->num_child >= 0) { // Dag does not actually store the number of children
    response->code = Concat("NL", "");
    response->data = Concat(request->region, "");
  } else if (mydag->state != OPEN) {
    response->code = Concat("RC", "");
    response->data = Concat(request->region, "");
  } else {
    struct candidates *vote_change = CreateCandidates(request->data);
    TrySubtraction(response, mydag, vote_change);
    free(vote_change);
  }
}

/**
 * @brief Adds a region to the dag.
 *
 * @brief The request detailing the command.
 * @brief The response of the successfullness.
 */
void Add_Region(struct request *request, struct response *response) {
  struct dag *mydag = get_dag(dag, request->region);
  struct dag *testdag = get_dag(dag, request->data);
  if (mydag == NULL) {
    response->code = Concat("NR", "");
    response->data = Concat(request->region, "");
  } else if (testdag != NULL) {
    response->code = Concat("UE", "");
    response->data = Concat("", "");
  } else {
    //printf("Adding child region %s to region %s\n", request->data, request->region);
    pthread_mutex_lock(&mydag->mutex);
    AddChildDag(mydag, request->data);
    pthread_mutex_unlock(&mydag->mutex);
    response->code = Concat("SC", "");
    response->data = Concat("Region added successfully", "");
  }
}

/**
 * @brief Calls the proper function on a request.
 *
 * @param The request structure;
 * @return The response structure.
 */
struct response *ExecuteRequest(struct request *request) {
  struct response *response = (struct response *)malloc(sizeof(struct response));
  response->data = NULL;
  response->code = NULL;
  if (!strcmp(request->code, "RW")) {
    Return_Winner(request, response);
  } else if (!strcmp(request->code, "CV")) {
    Count_Votes(request, response);
  } else if (!strcmp(request->code, "OP")) {
    Open_Polls(request, response);
  } else if (!strcmp(request->code, "AV")) {
    Add_Votes(request, response);
  } else if (!strcmp(request->code, "RV")) {
    Remove_Votes(request, response);
  } else if (!strcmp(request->code, "CP")) {
    Close_Polls(request, response);
  } else if (!strcmp(request->code, "AR")) {
    Add_Region(request, response);
  } else {
    response->code = Concat("UC", "");
    response->data = Concat(request->code, "");
  }
  return response;
}

/**
 * @brief Creates a request structure from an input character array.
 *
 * @param The input.
 * @return The request.
 */
struct request *CreateRequest(char *message) {
  struct request *request = (struct request *)malloc(sizeof(struct request));
  request->code = (char *)malloc(3 *sizeof(char));
  request->region = (char *)malloc(16 *sizeof(char));
  request->data = (char *)malloc(239 *sizeof(char));
	/////changed
	memset (request->code,'\0',(3 *sizeof(char)));
	memset (request->region,'\0',(16 *sizeof(char)));
	memset (request->data,'\0',(239 *sizeof(char)));
	/////changed
  sscanf(message,"%[^;];%[^;];%[^;]", request->code, request->region, request->data);
  int i;
  for (i=strlen(request->region) ; request->region[i] == ' ' || request->region[i] == '\0' ; i--);
  request->region[i+1] = '\0';
  return request;
}

/**
 * @brief Creates a character array from a response structure.
 *
 * @param The response.
 * @return The resulting character array.
 */
char *EncodeResponse(struct response *response) {
  char *message = (char *)malloc(2+strlen(response->code) + strlen(response->data));
  strcpy(message, response->code);
  strcpy(message+strlen(response->code), ";");
  strcpy(message+(strlen(response->code)+1), response->data);
  strcpy(message+(strlen(message)), "\0");
  return message;
}

/**
 * @brief deallocate request and response have been allocated
 */
void FreeRequestResponse(struct request *req, struct response *res){
	free(req->code);
	free(req->region);
	free(req->data);
	free(req);

	free(res->code);
	free(res->data);
	req = NULL;
	res = NULL;
}

/**
 * @brief This function details all of the actions that a child process is to undertake.
 */
void *threadFunction(void *arg){

	threadArg_pt args = (threadArg_pt)arg;
	int client_sockfd = args->client_fd;
	char req_buff[STRING_SIZE] = {""};
	int req_len = 0;
	while((req_len = read(client_sockfd, req_buff, STRING_SIZE)) != 0){
		struct request *req = CreateRequest(req_buff);
		printf("Request received from client at %s:%d, %s %s %s​\n",
					args->client_ip, args->client_port, req->code, req->region, req->data);
		struct response *res = ExecuteRequest(req);
		printf("Sending response to client at %s:%d, %s %s\n",
					args->client_ip, args->client_port, res->code, res->data);
		char *send_res = EncodeResponse(res);
		write(client_sockfd, send_res, STRING_SIZE);
		usleep(500);
		FreeRequestResponse(req,res);
	}
	printf("Closed connection with client at %s:%d\n",
				args->client_ip, args->client_port);
	close(client_sockfd);
	pthread_detach(pthread_self());
	return NULL;
}

/********************************************************************
 * Public Functions
 *******************************************************************/

int main(int argc, char **argv) {
	if (argc < NUM_ARGS + 1) {
			printf("Usage : ./server <DAG FILE> <Server Port>\n");
			exit(1);
	}
	// The DAG is read in.
	const char *dag_file = argv[1];
	char **output_line, **output, *content = NULL;
  int token_line =0, token =0;

  output_line = (char **)malloc(MAX_SIZE);
  output = (char **)malloc(MAX_SIZE);
  content = read_file(dag_file);

  if(content == NULL){
    printf("Error: The input DAG at %s could not be read.\nFurther execution is impossible.\n",argv[1]);
    exit(3);
  }
  dag = new_dag();

  // The contents of the DAG is read in by tokenization.
  token_line = tokenizer(content, "\n", output_line); /////tokenize by line
  for(int i =0 ; i < token_line; i++){
    token = tokenizer(output_line[i], ":", output); /////tokenize by line
    build_dag(&dag, output, token, NULL);
  }
  ////////////////////////free output
	free_output(output, token);
	free_output(output_line, token_line);
	free(content);
  content = NULL;

	//////////////////////Initiate TCP Connection
	int server_port = atoi(argv[2]); ////need error handling

	int server_sockfd = socket(AF_INET , SOCK_STREAM , 0);
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(server_port);
	int server_len = sizeof(server_addr);

	if(bind(server_sockfd, (struct sockaddr *) &server_addr, server_len) < 0){
		perror("Cannot Bind\n");
		exit(1);
	}

	if(listen(server_sockfd, MAX_CONNECTION) < 0){
		perror("Cannot Listen\n");
		exit(1);
	}
	printf("Server listening on port %d\n",server_port);

	while (1){
		struct sockaddr_in client_addr;

		socklen_t size = sizeof(struct sockaddr_in);
		int client_sockfd = accept(server_sockfd, (struct sockaddr*) &client_addr, &size);
		if(client_sockfd != -1){
			pthread_t thread;
			int cli_port = ntohs(client_addr.sin_port);
			char *addr_str = inet_ntoa(client_addr.sin_addr);
			printf("Connection initiated from client at %s:%d\n",addr_str,cli_port);
			threadArg_pt arg = (threadArg_pt)malloc(sizeof(threadArg_t));
			arg->client_ip = addr_str;
			arg->client_fd = client_sockfd;
			arg->client_port = cli_port;
			if(pthread_create(&thread, NULL, (void*)threadFunction, (void*)arg) < 0){
				fprintf(stderr,"Thread Create Error\n");
			}
		}
	}
	close(server_sockfd);
  free_dag(dag);
  dag = NULL;
  return 0;
}
