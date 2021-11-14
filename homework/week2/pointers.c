/*
 * Swaps the value of two ints outside of this function.
 */
void swap(int *x, int *y) {
  *x = *x ^ *y;
  *y = *x ^ *y;
  *x = *x ^ *y;
}

/*
 * Increments the value of an int outside of this function by one. 
 */
void inc(int *x) {
  (*x)++;
}

/*
 *  Returns the number of bytes in a string. Does not use strlen. 
 */
int len(char *s) {
	char *i = s;
	while (*i++);
	return i - s - 1;
}
