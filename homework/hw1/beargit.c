#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>

#include "beargit.h"
#include "util.h"

/* Implementation Notes:
 *
 * - Functions return 0 if successful, 1 if there is an error.
 * - All error conditions in the function description need to be implemented
 *   and written to stderr. We catch some additional errors for you in main.c.
 * - Output to stdout needs to be exactly as specified in the function description.
 * - Only edit this file (beargit.c)
 * - You are given the following helper functions:
 *   * fs_mkdir(dirname): create directory <dirname>
 *   * fs_rm(filename): delete file <filename>
 *   * fs_mv(src,dst): move file <src> to <dst>, overwriting <dst> if it exists
 *   * fs_cp(src,dst): copy file <src> to <dst>, overwriting <dst> if it exists
 *   * write_string_to_file(filename,str): write <str> to filename (overwriting contents)
 *   * read_string_from_file(filename,str,size): read a string of at most <size> (incl.
 *     NULL character) from file <filename> and store it into <str>. Note that <str>
 *     needs to be large enough to hold that string.
 *  - You NEED to test your code. The autograder we provide does not contain the
 *    full set of tests that we will run on your code. See "Step 5" in the homework spec.
 */

const char* NULL_COMMIT = "0000000000000000000000000000000000000000";
const char* GO_BEARS = "GO BEARS!";
const int GO_BEARS_LENGTH = 9;
const int NEW_PATH_SIZE = 50;
const int BUFFER_SIZE = FILENAME_SIZE + NEW_PATH_SIZE;

/* beargit init
 *
 * - Create .beargit directory
 * - Create empty .beargit/.index file
 * - Create .beargit/.prev file containing 0..0 commit id
 *
 * Output (to stdout):
 * - None if successful
 */

int beargit_init(void) {
  fs_mkdir(".beargit");

  FILE* findex = fopen(".beargit/.index", "w");
  fclose(findex);
  
  write_string_to_file(".beargit/.prev", NULL_COMMIT);

  return 0;
}


/* beargit add <filename>
 * 
 * - Append filename to list in .beargit/.index if it isn't in there yet
 *
 * Possible errors (to stderr):
 * >> ERROR: File <filename> already added
 *
 * Output (to stdout):
 * - None if successful
 */

int beargit_add(const char* filename) {
  FILE* findex = fopen(".beargit/.index", "r");
  FILE *fnewindex = fopen(".beargit/.newindex", "w");

  char line[FILENAME_SIZE];
  while(fgets(line, sizeof(line), findex)) {
    strtok(line, "\n");
    if (strcmp(line, filename) == 0) {
      fprintf(stderr, "ERROR: File %s already added\n", filename);
      fclose(findex);
      fclose(fnewindex);
      fs_rm(".beargit/.newindex");
      return 3;
    }

    fprintf(fnewindex, "%s\n", line);
  }

  fprintf(fnewindex, "%s\n", filename);
  fclose(findex);
  fclose(fnewindex);

  fs_mv(".beargit/.newindex", ".beargit/.index");

  return 0;
}


/* beargit rm <filename>
 * 
 * See "Step 2" in the homework 1 spec.
 *
 */

int beargit_rm(const char* filename) {
  int is_found = 0;
  FILE* findex = fopen(".beargit/.index", "r");
  FILE *fnewindex = fopen(".beargit/.newindex", "w");

  char line[FILENAME_SIZE];
  while(fgets(line, sizeof(line), findex)) {
    strtok(line, "\n");
    if (strcmp(line, filename) != 0)
      fprintf(fnewindex, "%s\n", line);
    else
      is_found = 1;
  }

  fclose(findex);
  fclose(fnewindex);
  
  if (is_found) {
    fs_mv(".beargit/.newindex", ".beargit/.index");
    return 0;
  } else {
    fs_rm(".beargit/.newindex");
    fprintf(stderr, "ERROR: File %s not tracked\n", filename);
    return 1;
  }
}

int is_go_bears(const char* msg) {
    
  for(int i = 0; i < GO_BEARS_LENGTH; i++)
    if (msg[i] != GO_BEARS[i])
      return 0;

  return 1;
}

int get_string_length(const char* str) {
    int length = 0;
    
    while(*str++)
      length++;
    return length;
}

int is_commit_msg_ok(const char* msg) {
  while(get_string_length(msg) >= GO_BEARS_LENGTH) {
    if (is_go_bears(msg))
      return 1;
    msg++;
  }
  return 0;
}

void init_commit_id(char* commit_id) {

    for (int i = 0; i < COMMIT_ID_BYTES; i++)
        commit_id[i] = 'c';
}

void increase_commit_id(char* commit_id) {

  for(int i = 0; i < COMMIT_ID_BYTES; i++) {
    char current = commit_id[i];
    
    switch (current) {
      case 'c':
        commit_id[i] = '1';
        return;
      case '1':
        commit_id[i] = '6';
        return;
      case '6':
        commit_id[i] = 'c';        
    }
  }
}

void next_commit_id(char* commit_id) {
  if (strcmp(commit_id, NULL_COMMIT)) {
      increase_commit_id(commit_id);
  } else {
      init_commit_id(commit_id);
  }
}

void copy_files(const char* msg, const char* commit_id) {
  char new_path[NEW_PATH_SIZE];
  char buffer[BUFFER_SIZE];
  sprintf(new_path, ".beargit/%s", commit_id);
  fs_mkdir(new_path);
  sprintf(buffer, "%s/.index", new_path);
  fs_cp(".beargit/.index", buffer);
  sprintf(buffer, "%s/.prev", new_path);
  fs_cp(".beargit/.prev", buffer);
  
  FILE* findex = fopen(".beargit/.index", "r");
  char line[FILENAME_SIZE];
  
  while(fgets(line, sizeof(line), findex)) {
    strtok(line, "\n");
    sprintf(buffer, "%s/%s", new_path, line);
    fs_cp(line, buffer);
  }
  fclose(findex);
  sprintf(buffer, "%s/.msg", new_path);
  write_string_to_file(buffer, msg);
  write_string_to_file(".beargit/.prev", commit_id);
}

/* beargit commit -m <msg>
 *
 * See "Step 3" in the homework 1 spec.
 *
 */

int beargit_commit(const char* msg) {
  if (!is_commit_msg_ok(msg)) {
    fprintf(stderr, "ERROR: Message must contain \"%s\"\n", GO_BEARS);
    return 1;
  }

  char commit_id[COMMIT_ID_SIZE];
  read_string_from_file(".beargit/.prev", commit_id, COMMIT_ID_SIZE);
  next_commit_id(commit_id);
  copy_files(msg, commit_id);

  return 0;
}

/* beargit status
 *
 * See "Step 1" in the homework 1 spec.
 *
 */

int beargit_status() {
  int counter = 0;
  
  fprintf(stdout, "Tracked files:\n\n");
  FILE* findex = fopen(".beargit/.index", "r");
  char line[FILENAME_SIZE];
  
  while(fgets(line, sizeof(line), findex)) {
    strtok(line, "\n");
    fprintf(stdout, "  %s\n", line);
    counter++;
  }
  
  fclose(findex);
  fprintf(stdout, "\n%d files total\n", counter);
  
  return 0;
}

void print_commits() {
    char buffer[BUFFER_SIZE];
    char msg[MSG_SIZE];
    char commit_id[COMMIT_ID_SIZE];
    read_string_from_file(".beargit/.prev", commit_id, COMMIT_ID_SIZE);
    
    while(strcmp(commit_id, NULL_COMMIT)) {
        sprintf(buffer, ".beargit/%s/.msg", commit_id);
        read_string_from_file(buffer, msg, MSG_SIZE);
        fprintf(stdout, "\ncommit %s\n    %s\n", commit_id, msg);
        sprintf(buffer, ".beargit/%s/.prev", commit_id);
        read_string_from_file(buffer, commit_id, COMMIT_ID_SIZE);
    }
    fprintf(stdout, "\n");
}

/* beargit log
 *
 * See "Step 4" in the homework 1 spec.
 *
 */

int beargit_log() {
  char commit_id[COMMIT_ID_SIZE];
  read_string_from_file(".beargit/.prev", commit_id, COMMIT_ID_SIZE);
  
  if (strcmp(commit_id, NULL_COMMIT)) {
    print_commits();
    return 0;
  }
  
  fprintf(stderr, "ERROR: There are no commits!\n");
  return 1;  
}
