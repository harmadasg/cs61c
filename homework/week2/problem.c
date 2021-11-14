/* Returns the sum of all the elements in SUMMANDS. */
int sum(int* summands, size_t n) {
  int sum = 0;
  for (int i = 0; i < n; i++)
    sum += summands[i];
  return sum;
}

/* Increments all the letters in the string STRING, held in an array of length N.
* Does not modify any other memory which has been previously allocated. */
void increment(char* string, int n) {
  for (int i = 0; i < n; i++)
    string[i]++;
}

/* Copies the string SRC to DST. */
void copy(char* src, char* dst) {
  while (*dst++ = *src++);
}
