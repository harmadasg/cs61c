/*
 * Returns the sum of all the numbers in the array.
 */
int foo(int *arr, size_t n) {
  return n ? arr[0] + foo(arr + 1, n - 1) : 0;
}

/*
 * Returns the number of 0s in the array and applies two's complement on the result.
 */
int bar(int *arr, size_t n) {
  int sum = 0, i;
  for (i = n; i > 0; i--) {
    sum += !arr[i - 1];
  }
  return ~sum + 1;
}

/*
 * Swaps the values of the two integers (call by value, nothing changes outside of the function).
 */
void baz(int x, int y) {
  x = x ^ y;
  y = x ^ y;
  x = x ^ y;
}
