/**
 * client.c
 *
 * by: Daniel Song, Isaac Stein
 *
 * last modified: 2018 Apr. 29
 *
 * This file details client functionality of our program.
 * This file also contains many of the functions used for send, receive and
    modify poll information or structure from or to a server.
 */

/********************************************************************
 * Includes
 *******************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>
#include "client_util.h"

#define NUM_ARGS 3


/********************************************************************
 * Candidate
 *******************************************************************/
struct candidates{
    int num_candidates;
    int max_candidates;
    char **names;
    int *votes;
};

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
/********************************************************************
 * Candidate
 *******************************************************************/

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
            return;
        }
    }
    if (i == d->max_candidates) LengthenCandidates(d);

    d->names[i] = malloc(sizeof(char) * strlen(name));
    strcpy(d->names[i], name);
    d->votes[d->num_candidates++] = num_votes;
}
void AddVote(struct candidates *d, char *name) { AddVotes(d, name, 1); }

/**
 * @brief Reads in the votes from some leaf file into a candidates structure.
 *
 * @param The candidate structure to read into.
 * @param The directory the file is contained in, including its name.
 */
void ReadLeafFile(struct candidates *d, char *dir) {
    if (d==NULL) return;
    FILE *file;
    if ((file = fopen(dir,"r")) == NULL) {
        fprintf(stderr, "Unable to read file at path %s\n",dir);
        return;
    }

    char *name = (char *)malloc(sizeof(char) * STRING_SIZE);
    while (fgets(name, STRING_SIZE, file)) {
        if(name != NULL && strcmp(name,"\n")){///ignore empty case
            strtok(name, "\n");
            AddVote(d, name);
        }
    }
    fclose(file);
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
  for (int i=0 ; i< d->num_candidates; i++){
    name_string = Concat(d->names[i],":");
    if(i != (d->num_candidates-1)){
      vote_string = Concat(itoa(d->votes[i]),",");
    }
    else{
      vote_string = Concat(itoa(d->votes[i]),NULL);
    }
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


int main(int argc, char** argv) {

    if (argc < NUM_ARGS + 1) {
        printf("Usage : ./client <REQ FILE> <Server IP> <Server Port>\n");
        exit(1);
    }

    const char *req_file = argv[1];
    char *server_ip = argv[2];
    if(!is_digit(argv[3])){
        printf("Invalid Input: Not a number\n");
        exit(1);
    }
    int server_port = atoi(argv[3]);

    // Create a TCP socket.
    printf("Initiated connection with server at %s:%d\n",server_ip,server_port);
    int client_sockfd = socket(AF_INET , SOCK_STREAM , 0);

    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr(server_ip);
    client_addr.sin_port = htons(server_port);
    int client_len = sizeof(client_addr);


    // Connect it.

    if (connect(client_sockfd, (struct sockaddr *) &client_addr, client_len) == 0){

      char *req_str = read_file(req_file);
      char *current_path = make_path(req_file);
      char **output = (char**)malloc(sizeof(char*)*STRING_SIZE);
      char **req_output = (char**)malloc(sizeof(char*)*STRING_SIZE);
      if(req_str == NULL) exit(1);
      else{
        int token = tokenizer(req_str, "\n", output);
        for(int i = 0 ; i < token ; i++){
          char req_buff[STRING_SIZE] = {""};
          int req_token = tokenizer(output[i], " ", req_output);
          char *request = req_output[0];
          if(request == NULL){
            perror("Request Cannot be NULL\n");
            exit(1);
          }

          if(strcmp(request,"Open_Polls") == 0){
            strcat(req_buff,"OP");
            strcat(req_buff,";");
            strcat(req_buff,req_output[1]);
            strcat(req_buff,";");
            printf("Sending request to server: OP​ %s\n",req_output[1]);
          }
          else if(strcmp(request,"Add_Votes") == 0){
            struct candidates *my_candidates = NewCandidates();
            char *path = Concat(current_path, req_output[2]);
            ReadLeafFile(my_candidates, path);
            strcat(req_buff,"AV");
            strcat(req_buff,";");
            strcat(req_buff,req_output[1]);
            strcat(req_buff,";");
            char *str = GetCandidatesString(my_candidates);
            strcat(req_buff,str);
            printf("Sending request to server: AV %s %s\n",req_output[1], str);
          }
          else if(strcmp(request,"Count_Votes") == 0){
            strcat(req_buff,"CV");
            strcat(req_buff,";");
            strcat(req_buff,req_output[1]);
            printf("Sending request to server: CV %s\n", req_output[1]);
          }
          else if(strcmp(request,"Remove_Votes") == 0){
            struct candidates *my_candidates = NewCandidates();
            char *path = Concat(current_path, req_output[2]);
            ReadLeafFile(my_candidates, path);
            strcat(req_buff,"RV");
            strcat(req_buff,";");
            strcat(req_buff,req_output[1]);
            strcat(req_buff,";");
            char *str = GetCandidatesString(my_candidates);
            strcat(req_buff,str);
            printf("Sending request to server: RV ​%s %s\n",req_output[1], str);
          }
          else if(strcmp(request,"Close_Polls") == 0){
            strcat(req_buff,"CP");
            strcat(req_buff,";");
            strcat(req_buff,req_output[1]);
            strcat(req_buff,";");
            printf("Sending request to server: CP​ %s\n",req_output[1]);
          }
          else if(strcmp(request,"Return_Winner") == 0){
            strcat(req_buff,"RW");
            strcat(req_buff,";");
            strcat(req_buff,";");
            printf("Sending request to server: RW\n");
          }
          ////extra credit
          else if(strcmp(request,"Add_Region") == 0){
            strcat(req_buff,"AR");
            strcat(req_buff,";");
            strcat(req_buff,req_output[1]);
            strcat(req_buff,";");
            strcat(req_buff,req_output[2]);
            printf("Sending request to server: AR %s %s\n",req_output[1],
                    req_output[2]);
          }
          else{
            fprintf(stderr,"Request Code Match Failed\n");
            exit(1);
          }
          //////write to server
          write(client_sockfd,req_buff,STRING_SIZE);

          free_output(req_output, req_token);
          char res_buff[STRING_SIZE] = {""};
          char res_code[2] = {""};
          char res_data[STRING_SIZE] = {""};

		      int msg_len = read(client_sockfd, res_buff, STRING_SIZE);
          if(msg_len > 0){
            sscanf(res_buff,"%[^;];%[^;]", res_code, res_data);
            printf("Received response from server: %s %s\n",
                  res_code, res_data);
          }

        }
        free_output(output, token);
        free(output);
        free(req_output);
      }
      free(current_path);
        // Close the file.
        printf("Closed connection with server at %s:%d\n",
              server_ip, server_port);
        close(client_sockfd);

    } else {
        perror("Connection failed!\n");
        exit(1);
    }


}
