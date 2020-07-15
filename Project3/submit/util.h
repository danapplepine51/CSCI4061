/**
 * util.h
 *
 * by: Daniel Song, Isaac Stein
 *
 * last modified: 2018 Apr. 12
 *
 * This file details common methods to be used across functions in this assignment.
 */

#define MAX_SIZE 4096

/**
 * @brief Takes in two string and returns a pointer to a new string which is their concatonation.
 *
 * @param The first string.
 * @param The second string.
 *
 * @return A pointer to the concatonated string.
 */
char *Concat(const char *s1, const char *s2) {
    if (s1 == NULL && s2 == NULL) return NULL;

    char *output;
    if (s1 != NULL && s2 == NULL) {
        output = (char *)malloc((strlen(s1) * sizeof(char)));
        strcpy(output, s1);
    } else if (s1 == NULL && s2 != NULL) {
        output = (char *)malloc((strlen(s2) * sizeof(char)));
        strcpy(output, s2);
    } else {
        output = (char *)malloc((strlen(s1)+strlen(s2)+1 * sizeof(char)));
        strcpy(output, s1);
        strcat(output, s2);
    }
    return output;
}


/**
 * @brief Creates a string representation of an integer.
 *        Only works on non-negative integers.
 *
 * @param The integer to represent.
 *
 * @return The string representation.
 */
char *itoa(int input) {
  if (input<0) input=0;
  int index, temp;
  char *output;
  temp = input;
  for (index=2 ; temp>=10 ; temp /= 10, index++);
  output = (char *)malloc(sizeof(char) * index);
  output[index-1] = '\0';
  for (int i=index-2 ; input>0 ; input /= 10, i--) output[i] = (input % 10) + '0';
  return output;
}

/**
 * @brief Gets the winner of an election from some input file.
 *        In the event of a tie, the first canidate read in is simply the winner.
 *
 * @param A file which lists the possible canidates.
 */
void GetWinner(FILE *file) {
    // This still uses the same functionality as the last assignment.
    // Thus, this may need to be modified for this assignment.
    if (file == NULL) return;
    char *top_name, *current_name;
    int top_votes, num_votes;
    top_name = (char *)malloc(MAX_SIZE * sizeof(char));
    current_name = (char *)malloc(MAX_SIZE * sizeof(char));
    top_votes = 0;

    // Each canidate is considered, with the largest entrant stored in top_name.
    while(fscanf(file, "%[^:]:%d\n", current_name, &num_votes) != EOF) {
        if (num_votes > top_votes) {
            top_votes = num_votes;
            strcpy(top_name, current_name);
	}
    }

    free(current_name);
    fprintf(file,"WINNER:%s\n", top_name);
    free(top_name);
}

/**
 * @brief check whether the path is a directory or folder
 *
 * @param Path of a file or directory.
 *
 * @return 1 if directory otherwise 0.
 */
int is_dir(const char *path){
  struct stat stb;
  if (stat(path, &stb) != 0)
       return 0;
   return S_ISDIR(stb.st_mode);
}

/**
 * @brief Remove the directory and its sub directroy
 *
 * @param Path of the directory to remove.
 *
 */
void rmdir_recurive(const char *path){

  struct dirent *dp;
    DIR *dir;

  if((dir = opendir(path)) == NULL){
      remove(path);
    }
    else{
      while((dp = readdir(dir)) != NULL){
          if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) continue;

          if(dp->d_type == DT_REG){
              char *tmp = malloc(sizeof(char)*MAX_SIZE);
              tmp[0] = '\0';
              strcat(tmp, path);
              strcat(tmp, "/");
              strcat(tmp, dp->d_name);
              //printf("%s\n",tmp);
              remove(tmp);
              free(tmp);
              tmp = NULL;
    }

          else if(dp->d_type == DT_DIR){
              char *tmp = malloc(sizeof(char)*MAX_SIZE);
              tmp[0] = '\0';
              strcat(tmp, path);
              strcat(tmp, "/");
              strcat(tmp, dp->d_name);
              //printf("%s\n",tmp);
              rmdir_recurive(tmp);
              remove(tmp);
              free(tmp);
              tmp = NULL;
            }
    }
  }
}

/**
 * @brief Read the file from filename
 *
 * @param Name of the file to read
 *
 * @return strings read from the file.
 */
char *read_file(const char *filename){

    FILE *fp = fopen(filename,"r");
    char *str = NULL;
    if(fp == NULL){
        perror("Cannot open file.");
        exit(3);
    }
    else{
        str = (char*)malloc(sizeof(char)*MAX_SIZE);
        char *c;
        str[0] = '\0';
        while(!feof(fp)){
            char tmp[MAX_SIZE/4];
            c = fgets(tmp, MAX_SIZE, fp);
            if(c != NULL){
                strcat(str, c);
            }
        }
	fclose(fp);
            str[strlen(str)+1] = '\0';

            if(strcmp(str, "") == 0){
                return NULL;
            }
            else{
                return str;
            }
    }
    return NULL;
}

/**
 * @brief Tokenize the input strings
 *
 * @param strings to tokenize.
 * @param deliminator to tokenize.
 * @param output string pointer to store tokenized string.
 *
 * @return The number of token
 */
int tokenizer(char *str, char *delim ,char **output){

    int token =0;
    char *copy = malloc(strlen(str)+1);

    strncpy(copy, str, strlen(str)+1);

    char *tmp = strtok(copy, delim);

    for(int i=0; tmp!=NULL; i++){
        output[i] = (char*)malloc(strlen(tmp)+2);
        strncpy(output[i], tmp, strlen(tmp)+1);
        //printf("%d : %s\n",&(output[i]),output[i]);
        token++;
        tmp = strtok(NULL, delim);
    }
    free(copy);
    copy = NULL;

    return token;
}
