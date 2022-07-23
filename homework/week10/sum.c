#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <emmintrin.h> /* where intrinsics are defined */


#define CLOCK_RATE_GHZ 2.0e9   /* just a wild guess ... */

/* Time stamp counter */
static __inline__ unsigned long long RDTSC(void) {
    unsigned hi,lo;
    __asm__ volatile("rdtsc" : "=a"(lo),"=d"(hi));
    return ((unsigned long long) lo)| (((unsigned long long)hi) << 32);
}

int sum_naive( int n, int *a )
{
    int sum = 0;
    for( int i = 0; i < n; i++ )
        sum += a[i];
    return sum;
}

int sum_unrolled( int n, int *a )
{
    int sum = 0;

    /* do the body of the work in a faster unrolled loop */
    for( int i = 0; i < n/4*4; i += 4 )
    {
        sum += a[i+0];
        sum += a[i+1];
        sum += a[i+2];
        sum += a[i+3];
    }

    /* handle the small tail in a usual way */
    for( int i = n/4*4; i < n; i++ )   
        sum += a[i];

    return sum;
}

int sum_vectorized( int n, int *a )
{
    int* sum;
    __m128i *address, addends, vector_sum;
    vector_sum = _mm_setzero_si128(); // 128 bit vector with zeros

    /* handle the small tail in a usual way */
    sum = (int*) &vector_sum;
    for( int i = n/4*4; i < n; i++ )
        sum[0] += a[i];

     for(int i = 0; i < n/4*4; i += 4 ) {
        address = (__m128i*) (a + i);
        addends = _mm_loadu_si128(address); // load 4 integers into a 128 bit vector
        vector_sum = _mm_add_epi32(vector_sum, addends); // add the content of the above vector to the sum vector
     }

    return sum[0] + sum[1] + sum[2] + sum[3];

}


int sum_vectorized_unrolled( int n, int *a )
{
    int* sum;
    __m128i *address, addends, vector_sum;
    vector_sum = _mm_setzero_si128();

    sum = (int*) &vector_sum;
    for( int i = n/16*16; i < n; i++ )
        sum[0] += a[i];

     for(int i = 0; i < n/16*16; i += 16 ) {
        address = (__m128i*) (a + i);
        addends = _mm_loadu_si128(address);
        vector_sum = _mm_add_epi32(vector_sum, addends);

        address = (__m128i*) (a + i + 4);
        addends = _mm_loadu_si128(address);
        vector_sum = _mm_add_epi32(vector_sum, addends);

        address = (__m128i*) (a + i + 8);
        addends = _mm_loadu_si128(address);
        vector_sum = _mm_add_epi32(vector_sum, addends);

        address = (__m128i*) (a + i + 12);
        addends = _mm_loadu_si128(address);
        vector_sum = _mm_add_epi32(vector_sum, addends);
     }

    return sum[0] + sum[1] + sum[2] + sum[3];
}

void benchmark( int n, int *a, int (*computeSum)(int,int*), char *name )
{
    /* warm up */
    int sum = computeSum( n, a );

    /* measure */
    unsigned long long cycles = RDTSC();
    sum += computeSum( n, a );
    cycles = RDTSC()-cycles;
    
    double microseconds = cycles/CLOCK_RATE_GHZ*1e6;
    
    /* report */
    printf( "%20s: ", name );
    if( sum == 2*sum_naive(n,a) ) 
        printf( "%.2f microseconds\n", microseconds );
    else
        printf( "ERROR!\n" );
} 

int main( int argc, char **argv )
{
    const int n = 7777; /* small enough to fit in cache */
    
    /* init the array */
    int a[n] __attribute__ ((aligned (32))); /* align the array in memory by 32 bytes (good for 256 bit intrinsics) */
    for( int i = 0; i < n; i++ ) a[i] = rand( );
    
    /* benchmark series of codes */
    benchmark( n, a, sum_naive, "naive" );
    benchmark( n, a, sum_unrolled, "unrolled" );
    benchmark( n, a, sum_vectorized, "vectorized" );
    benchmark( n, a, sum_vectorized_unrolled, "vectorized unrolled" );

    return 0;
}
