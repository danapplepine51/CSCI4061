/**
 * votecounter.c
 *
 * by: Daniel Song, Isaac Stein
 *
 * last modified: 2018 Apr. 12
 *
 * This file details main functionality of our program.
 * This file also contains many of the functions used for modifying
 *   the DAG, candidate information, directories, and strings.
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
#include "util.h"
#include "p_queue.h"


#define PDAG (*dag)

/********************************************************************
 * Structures
 *******************************************************************/
typedef struct dag{
    struct dag *parent;
    char *name;
    char *dir;
    int num_child;
    int index;
    struct dag **child;
    pthread_mutex_t mutex;
}dag_t, *dag_pt;

struct candidates{
  int num_candidates;
  int max_candidates;
  char **names;
  int *votes;
};

/********************************************************************
 * Globals
 *******************************************************************/
struct p_queue *queue;
pthread_mutex_t queue_lock;

struct dag *dag;

const char *input_dir;
const char *output_dir;

char *log_dir;
pthread_mutex_t log_lock;


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
    vote_string = Concat(itoa(d->votes[i]),"\n");
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

void create_output_dir(dag_pt dag,const char *path){
    if(dag == NULL){
        return;
    }
    char *tmp = malloc(sizeof(char)*100);
    tmp[0] = '\0';
    strcat(tmp,path);
    strcat(tmp,"/");
    strcat(tmp,dag->name);
    //char *str = dag->name;
    dag->dir = malloc(sizeof(char)*strlen(tmp));
    dag->dir[0] = '\0';
    strcpy(dag->dir, tmp);
    //printf("This is DAG : %s PATH : %s\n", dag->name, dag->dir);
    if(tmp !=NULL)
        mkdir(tmp, 0777);
    for(int i =0; i < dag->num_child; i++){
        create_output_dir(dag->child[i], tmp);
    }
}/**
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

/**
 * @brief Adds a vote to a candidate.
 *
 * @param The candidates structure to work on.
 * @param The name of the candidate to add the vote to.
 */
void AddVote(struct candidates *d, char *name) { AddVotes(d, name, 1); }

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
    tmp->dir = NULL;
    tmp->name = NULL;
    tmp->num_child = -1;
    tmp->index = -1;
    tmp->child = NULL;
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
                if((find = get_dag(PDAG, node)) == NULL) { /////parent with no dag
                    input_dag->name = malloc(sizeof(char) * strlen(node));
                    strcpy(input_dag->name,node);
                } else { /////parent with initialized dag
                    input_dag = find;
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
        printf("Name:%s ",dag->name);
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

/********************************************************************
 * Directory Functions
 *******************************************************************/
/**
 * @brief Writes a message to the global log file.
 *
 * @param The file being edited.
 * @param The number of the thread editing the file.
 * @param The status of the thread.
 */
void write_to_log(char *file_name, int tid, char *status) {
  pthread_mutex_lock(&log_lock);

  FILE *log = fopen(log_dir, "a");
  if (log == NULL) {
    fprintf(stderr, "Failed to print message:\n  %s:%d:%sto log at dir:\n  %s\n", file_name, tid, status, log_dir);
    pthread_mutex_unlock(&log_lock);
    return;
  }
  fprintf(log, "%s:%d:%s\n", file_name, tid, status);

  fclose(log);
  pthread_mutex_unlock(&log_lock);
}

/**
 * @brief Decodes a character array according to some standard function.
 *        On NULL input, a NULL pointer is returned.
 *
 * @param The array to decode.
 *
 * @return The decoded array.
 */
char *Decode(char *input) {
    if (input == NULL) return input;
    char *output = malloc(strlen(input) * sizeof(char));
    int i;

    for (i=0 ; input[i] !='\0'; i++) {
        if (input[i] >= 'a' && input[i] <= 'z') {
            output[i] = ((input[i] + 2 - 'a') % ('z' - 'a' + 1)) + 'a';
        } else if (input[i] >= 'A' && input[i] <= 'Z') {
            output[i] = ((input[i] + 2 - 'A') % ('Z' - 'A' + 1)) + 'A';
        } else {
            output[i] = input[i];
        }
    }
    output[i] = '\0';

    return output;
}

/**
 * @brief Decodes a character array according to some standard function.
 *        The input string is destructively modified to become the answer.
 *
 * @param The array to decode.
 */
void DecodeInPlace(char *input) {
    if (input == NULL) return;

    for (int i=0 ; input[i] !='\0'; i++) {
        if (input[i] >= 'a' && input[i] <= 'z') {
            input[i] = ((input[i] + 2 - 'a') % ('z' - 'a' + 1)) + 'a';
        } else if (input[i] >= 'A' && input[i] <= 'Z') {
            input[i] = ((input[i] + 2 - 'A') % ('Z' - 'A' + 1)) + 'A';
        }
    }
}
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

  char *name = (char *)malloc(sizeof(char) * MAX_SIZE);
  while (fgets(name, MAX_SIZE, file)) {
    if(name != NULL && strcmp(name,"\n")){///ignore empty case
      DecodeInPlace(strtok(name,"\n"));
      AddVote(d, name);
    }
  }
  fclose(file);
}

/**
 * @brief Write the decrypted leaf file.
 *
 *
 */
void WriteLeafFile(struct candidates *d, char *dir) {
  FILE *file;
  if ((file = fopen(dir,"w")) == NULL) {
    return;
  }
  for (int i = 0; i<d->num_candidates; i++) {
    for(int j=0; j<d->votes[i] ; j++) {
      fprintf(file, "%s", d->names[i]);
      if (!(j==d->votes[i]-1 && i==d->num_candidates-1)) {
	fprintf(file, "\n");
      }
    }
  }
  fclose(file);
}

/**
 * @brief Writes the information about a vote count to an output file.
 *        This should ONLY ever be called as an auxillary function to ReadNode.
 *
 * @param The structure to count the votes of.
 * @param The directory of the file to write.
 */
void WriteFile(struct candidates *d, char *dir) {
  // This is an auxillary function to ReadNode.
  // Do NOT call this on its own.
  char *message = GetCandidatesString(d);
  FILE *file;
  if ((file = fopen(dir,"w")) == NULL) {
    fprintf(stderr,"Failed to open file %s\ncannot write message %s",dir, message);
    free(message);
    return;
  }
  fprintf(file, "%s", message);
  free(message);
  fclose(file);
}

/**
 * @brief Reads in the votes from some aggregate file into a candidates structure.
 *        This also Aggregates the votes and rewrites the new file.
 *
 * @param The candidate structure to read into.
 * @param The directory the file is contained in, including its name.
 * @param The mutex lock of the file, necessary for proper parallelism.
 */
void ReadNode(struct candidates *d, char *dir, pthread_mutex_t *mutex) {
  if (d==NULL) return;
  pthread_mutex_lock(mutex);
  struct candidates *dc = CandCopy(d);
  FILE *file;
  if ((file = fopen(dir,"r")) == NULL) {
    //printf("fail to read file %s\n",dir);
    WriteFile(d, dir);
    pthread_mutex_unlock(mutex);
    return;
  }
  //char **args = (char **)malloc(sizeof(char) * 2);
  char *name = (char *)malloc(sizeof(char) * MAX_SIZE);
  int num;
  while (fscanf(file, "%[^:]:%d\n",name,&num)>0) {
    //strtok_r(name,"\n");
    //tokenizer(name, ":", args);
    AddVotes(dc, name, num);
    //free(args[0]);
    //free(args[1]);
  }
  fclose(file);
  WriteFile(dc, dir);
  //free(args);
  free(name);
  FreeCandidates(dc);
  pthread_mutex_unlock(mutex);
}

/********************************************************************
 * String Functions
 *******************************************************************/
/**
 * @brief Makes a new executable path to a file in the dir path.
 *
 * @param The path with old executable.
 * @param The new executable.
 *
 * @return The new path.
 */
char *replace_dir(char *path, char *name) {
    int path_length = strlen(path);
    int name_length = strlen(name);
    char *arr_out = (char *)malloc((path_length+name_length) * sizeof(char));
    int last_position;
    for (last_position = path_length-2 ; path[last_position] != '/' && last_position>=0 ; last_position--);
    last_position++;
    strcpy(arr_out, path);
    strcpy(arr_out+last_position, name);
    return arr_out;
}

/**
 * @brief Takes in a full directory path and cuts it down to the last directory in its path.
 *
 * @param A full directory string.
 */
void last_dir(char *dir) {
    // If the directory is empty or of a length of 1 then we cannot trim off anything.
    if (strlen(dir) < 2) return;
    int dirlength = strlen(dir);
    // We go backwards through th string, looking for the last position of '/'
    // Dir is guaranteed to not have a trailing '/', so the length after the character will be non-trivial
    int last_position, replace_position;
    for (last_position = dirlength-1 ; dir[last_position] != '/' && last_position>=0 ; last_position--);
    // If the last position is -1, then there are no '/'s in the dir.
    if (last_position == -1) return;
    last_position++;
    for (replace_position = 0 ; (last_position+replace_position) < dirlength ; replace_position++) dir[replace_position] = dir[last_position+replace_position];
    dir[replace_position] = '\0';
}

/**
 * @brief Returns the last dir in a path.
 *
 * @param A full directory string.
 *
 * @return The name of the last dir in the string.
 */
char *get_last_dir(char *dir) {
    // If the directory is empty or of a length of 1 then we cannot trim off anything.
    if (strlen(dir) < 2) return NULL;
    int dirlength = 0;
    // We go backwards through th string, looking for the last position of '/'
    // Dir is guaranteed to not have a trailing '/', so the length after the character will be non-trivial
    int last_position;
    for (last_position = strlen(dir)-1 ; dir[last_position] != '/' && last_position>=0 ; last_position--, dirlength++);

    char *new_dir = (char *)malloc(sizeof(char) * (dirlength+2));
    new_dir[dirlength--] = '\0';
    for (last_position = strlen(dir)-1 ; dir[last_position] != '/' && last_position>=0 ; last_position--, dirlength--)
      new_dir[dirlength] = dir[last_position];

    return new_dir;
}

/**
 * @brief Gets the dir path for some output file from an input dir.
 *
 * @param The dir to get the output from.
 *
 * @return The output name.
 */
char *get_output_name(char *dir) {
  char *file_name, *full_file, *t_output, *output;
  file_name = get_last_dir(dir);
  full_file = Concat(file_name, ".txt");
  free(file_name);
  t_output = Concat(dir, "/");
  output = Concat(t_output, full_file);
  free(t_output);
  free(full_file);
  return output;
}

/**
 * @brief Build an priority queue from the path
 *
 * @param Input path of information.
 * @param Queue to build queue
 *
 * @return The name of the element removed.
 */
int build_queue(const char *path, struct p_queue *queue, dag_pt dag){
    struct dirent *dp;
    DIR *dir;
    int count = 0;
    dag_pt find = NULL;

    if((dir = opendir(path)) == NULL){
        perror ("Cannot open directory");
        exit(7);
    }

    while((dp = readdir(dir)) != NULL){
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) continue;

        if(dp->d_type == DT_REG){
            if((find = get_dag(dag, dp->d_name)) != NULL){
                char *file_name = Concat(dp->d_name, "");
                char *check_prio = Concat(dp->d_name, "");
                char *p_temp = strstr(check_prio, "_p_");
                if(p_temp == NULL){////default priority
                    enqueue(queue, file_name, INT_MAX);
                }else{
                  char *tok = strrchr(p_temp, 'p');
                  tok = strtok(tok, "_");
                  while (tok != NULL){
                    if(tok[0] == 'p'){
                      //printf("yes"); ///ignore
                    }else{
                      int priority = atoi(tok);
                      //printf("%d\n",prioirty);
                      enqueue(queue, file_name, priority);
                    }
                    tok = strtok (NULL, "_");
                  }
                }
                count++;
            }
            find = NULL;
        }
    }
    closedir(dir);
    return count;
}

/**
 * @brief Takes in a dag, and writes the votes of its candidates to it.
 *        This also cascades those votes up through the dag's ancestors.
 *
 * @param The dag to write the votes of.
 * @param The candidate structure to write.
 */
void CascadeWrites(struct dag *dag, struct candidates *d) {
  if (dag == NULL) return;
  char *out_name = get_output_name(dag->dir);
  ReadNode(d, out_name, &(dag->mutex));
  free(out_name);

  CascadeWrites(dag->parent, d);
}

/**
 * @brief This function details all of the actions that a child process is to undertake.
 */
void *child_function() {
  // The entire function loops until in find that queue is empty
  char *file_dir = NULL;
  char *file_name = NULL;
  struct candidates *my_candidates = NULL;
  while (1) {
    // 1. Retrieve file name from queue
    pthread_mutex_lock(&queue_lock);
    file_name = dequeue(queue);
    pthread_mutex_unlock(&queue_lock);
    // dequeue returns NULL when the queue is empty
    // This means that the queue is empty and the
    //   execution is complete.
    if (file_name == NULL) return NULL;
    //printf("here with %s\n",file_name);

    struct dag *my_dag = NULL;
    my_dag = get_dag(dag, file_name);

    // If the name is not found in the DAG then ignore the error and continue on.
    if (my_dag == NULL) {
      fprintf(stderr,"Error: file name %s pulled from queue not found in DAG!\n", file_name);
      continue;
    }
    if (my_dag->num_child != -1) { // I have no idea why num_child does not actually store the number of children.
      fprintf(stderr,"Error: file %s is not actually a leaf node! Ignoring input.\n", file_name);
      continue;
    }


    // Append status to log.txt
    write_to_log(file_name, pthread_self(), "start\n");

    // The leaf file is read and decrypted
    my_candidates = NewCandidates();
    char *temp = Concat(input_dir, "/");
    file_dir = Concat(temp, file_name);
    free(temp);
    ReadLeafFile(my_candidates, file_dir);

    // The output is written
    char *out_name = get_output_name(my_dag->dir);
    WriteLeafFile(my_candidates, out_name);
    free(out_name);
    CascadeWrites(my_dag->parent, my_candidates);

    // Append status to log.txt
    write_to_log(file_name, pthread_self(), "end\n");
  }
}

/********************************************************************
 * Public Functions
 *******************************************************************/

int main(int argc, char **argv) {
  if (argc > 5 || argc < 4) {
    // A bit of an ugly error message, but it gets the job done
    fprintf(stderr, "Error: Incorrct number of arguments: Expected 3-4, Got %d\n"
          "proper arguments are: DAG, input_dir, output_dir, <num_threads>\n", argc-1);
    exit(1);
  }
  const char *dag_file;
  int num_threads;

  dag_file = argv[1];
  input_dir = argv[2];
  output_dir = argv[3];
  // Does not check that the input is less than INT_MAX, may overflow into smaller positives on large input
  num_threads = (argc == 5 && atoi(argv[4]) > 0) ? atoi(argv[4]) : 4;

  pthread_t *threads;
  pthread_attr_t custom_attr;
  threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
  pthread_attr_init(&custom_attr);

  // The DAG is read in.
  int token_line, token;
  char **output_line, **output, *content = NULL;
  token_line = token = 0;
  int check = is_dir(dag_file);
  if(check){/////dag_file is dir
    fprintf(stderr,"Directory, Input valid file name.\n");
    exit(3);
  }

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
    build_dag(&dag, output, token, output_dir);
  }
  ////////////////////////free output
  for(int i =0 ; i < token; i++){
    //printf("%d : %s\n",&output[i],output[i]);
    free(output[i]);
  }
  free(output);
  output = NULL;

  for(int i =0 ; i < token_line; i++){
    //printf("%s\n",output_line[i]);
    free(output_line[i]);
  }
  free(output_line);
  output_line = NULL;

  free(content);
  content = NULL;

  // The ouptut directory is created
  int acc = access(output_dir, F_OK);
    if(acc == 0){///folder exist
        char *path_rm = malloc(sizeof(char)*MAX_SIZE);
        strcpy(path_rm, output_dir);
        rmdir_recurive(path_rm);
  }
  mkdir(output_dir,0777);
  create_output_dir(dag, output_dir);

  // The log directory is created
  log_dir = Concat(output_dir, "/log.txt");

  // The queue is created
  queue = NewQueue();

  // The contents of the queue are read in from the input directory
  build_queue(input_dir, queue, dag);

  // If the queue is empty, we are done
  if (queue->head == NULL) {
    fprintf(stderr,"error: input directory is empty\n");
    return 0;
  }

  // Thread specific activites are executed
  for (int i=0 ; i<num_threads ; i++) {
    pthread_create(&threads[i],
		   &custom_attr,
		   (void *)child_function,
		   (void *)NULL);
  }
  for (int i=0 ; i<num_threads ; i++) pthread_join(threads[i], NULL);

  // The winner is decided
  char *winner_file = get_output_name(dag->dir);
  FILE *outfile;
  if ((outfile = fopen(winner_file, "a+")) == NULL) {
    fprintf(stderr,"Failed to write winnde output to file %s\n", winner_file);
  } else {
    GetWinner(outfile);
    fclose(outfile);
  }

  free(winner_file);
  free(threads);
  free_dag(dag);
  dag = NULL;
  return 0;
}
