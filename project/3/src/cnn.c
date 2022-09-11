#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>

// Include SSE intrinsics
#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#include <immintrin.h>
#include <x86intrin.h>
#endif

// Include OpenMP
#include <omp.h>


#define L0_DEPTH 3
#define L0_SX 32
#define L0_SY 32
#define FILTER_SX 5
#define FILTER_SY 5
#define L0_FILTERS 16
#define STRIDE 1
#define PAD 2
#define L3_SX 16
#define L3_SY 16
#define L3_FILTERS 20
#define L3_DEPTH 16
#define L6_SX 8
#define L6_SY 8
#define L6_FILTERS 20
#define L6_DEPTH 20

// Helper functions -----------------------------------------------------------

/*
 * Get a current timestamp with us accuracy. This will give you the time that
 * has passed since a certain point in time. While the value itself doesn't
 * tell you much, you can subtract timestamps from each other to get the
 * amount of time that has passed between them.
 */

static inline uint64_t timestamp_us() {
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return 1000000L * tv.tv_sec + tv.tv_usec;
}

// Vol ------------------------------------------------------------------------

// Volumes are used to represent the activations (i.e., state) between the
// different layers of the CNN. They all have three dimensions. The inter-
// pretation of their content depends on the layer that produced them. Before
// the first iteration, the Volume holds the data of the image we want to
// classify (the depth are the three color dimensions). After the last stage
// of the CNN, the Volume holds the probabilities that an image is part of
// a specific category.

/*
 * Represents a three-dimensional array of numbers, and its size. The numbers
 * at (x,y,d) are stored in array w at location ((v->sx * y)+x)*v->depth+d.
 */

typedef struct vol {
  uint64_t sx,sy,depth;
  double* w;
} vol_t;

/*
 * Set the value at a specific entry of the array.
 */

static inline double get_vol(vol_t* v, int x, int y, int d) {
  return v->w[((v->sx * y)+x)*v->depth+d];
}

/*
 * Get the value at a specific entry of the array.
 */

static inline void set_vol(vol_t* v, int x, int y, int d, double val) {
  v->w[((v->sx * y)+x)*v->depth+d] = val;
}

/*
 * Allocate a new array with specific dimensions and default value v.
 */

static vol_t* make_vol(int sx, int sy, int d, double v) {
  vol_t* out = (vol_t*)malloc(sizeof(struct vol));
  out->w = (double*)malloc(sizeof(double)*(sx*sy*d));
  out->sx = sx;
  out->sy = sy;
  out->depth = d;

  for (int x = 0; x < sx; x++)
    for (int y = 0; y < sy; y++)
      for (int z = 0; z < d; z++)
        set_vol(out, x, y, z, v);

  return out;
}

/*
 * Copy the contents of one Volume to another (assuming same dimensions).
 */

static vol_t* copy_vol(vol_t* dest, vol_t* src) {
  for (int x = 0; x < dest->sx; x++)
    for (int y = 0; y < dest->sy; y++)
      for (int z = 0; z < dest->depth; z++)
        set_vol(dest, x, y, z, get_vol(src, x, y, z));
}

/*
 * Deallocate the array.
 */
void free_vol(vol_t* v) {
  free(v->w);
  free(v);
}

// A note about layers --------------------------------------------------------

/*
 * What follows are the different layers of the CNN. You will not have to
 * understand what these layers are actually doing. In general terms, each
 * layer performs a "forward" operation on a batch of inputs. During this
 * forward operation, the layer takes a set of input Volumes and transforms
 * them into a set of output Volumes (one output for each input). What differs
 * is the operation performed by each layer.
 *
 * In addition to the _forward function, each layer also provides a data
 * structure, holding (fixed) parameters for that layer, a make_ function to
 * allocate an instance of the layer with a particular set of parameters and
 * a load function to load training data for that layer from a file. Note that
 * you will not have to make any changes to any of these functions. The only
 * function you need to consider is the _forward function.
 */

// Convolutional Layer --------------------------------------------------------

typedef struct conv_layer {
  // required
  int out_depth;
  int sx;
  int in_depth;
  int in_sx;
  int in_sy;

  // optional
  int sy;
  int stride;
  int pad;
  double l1_decay_mul;
  double l2_decay_mul;

  // computed
  int out_sx;
  int out_sy;
  double bias;
  vol_t* biases;
  vol_t** filters;
} conv_layer_t;

conv_layer_t* make_conv_layer(int in_sx, int in_sy, int in_depth,
                              int sx, int filters, int stride, int pad) {
  conv_layer_t* l = (conv_layer_t*)malloc(sizeof(conv_layer_t));

  // required
  l->out_depth = filters;
  l->sx = sx;
  l->in_depth = in_depth;
  l->in_sx = in_sx;
  l->in_sy = in_sy;
    
  // optional
  l->sy = l->sx;
  l->stride = stride;
  l->pad = pad;
  l->l1_decay_mul = 0.0;
  l->l2_decay_mul = 1.0;

  // computed
  l->out_sx = floor((l->in_sx + l->pad * 2 - l->sx) / l->stride + 1);
  l->out_sy = floor((l->in_sy + l->pad * 2 - l->sy) / l->stride + 1);

  l->filters = (vol_t**)malloc(sizeof(vol_t*)*filters);
  for (int i = 0; i < filters; i++) {
    l->filters[i] = make_vol(l->sx, l->sy, l->in_depth, 0.0);
  }

  l->bias = 0.0;
  l->biases = make_vol(1, 1, l->out_depth, l->bias);

  return l;
}

void calc_sum(vol_t* f, vol_t* V, int fx, int fy, int ox, int oy, __m256d* vector_sum) {
  if(oy >= 0 && oy < L0_SY && ox >=0 && ox < L0_SX) {
    int f_start = ((FILTER_SX * fy)+fx) * L0_DEPTH;
    int V_start = ((L0_SX * oy)+ox) * L0_DEPTH;

    __m256d multiplicand = _mm256_loadu_pd(f->w + f_start);
    __m256d multipllicator = _mm256_loadu_pd(V->w + V_start);
    __m256d product = _mm256_mul_pd(multiplicand, multipllicator);
    *vector_sum = _mm256_add_pd(*vector_sum, product);
  }
}

// The conv_forward_1 funciton handles the range 2..30, 2..30. Here the other cases have to be covered including
//  0..2,   0..32
//  0..32,  0..2
//  30..32, 0..32
//  0..32,  30..32
// With a bit more code all the 4 variations are done through only one loop
void handle_corner_cases(vol_t* f, vol_t* V, vol_t* A, conv_layer_t* l, int d) {
  for(int ay = 0; ay < 2; ay++) {
    for(int ax = 0; ax < 32; ax++) {
      __m256d vector_sum0 = _mm256_setzero_pd();
      __m256d vector_sum1 = _mm256_setzero_pd();
      __m256d vector_sum2 = _mm256_setzero_pd();
      __m256d vector_sum3 = _mm256_setzero_pd();
      double*  sum0 = (double*) &vector_sum0;
      double*  sum1 = (double*) &vector_sum1;
      double*  sum2 = (double*) &vector_sum2;
      double*  sum3 = (double*) &vector_sum3;
      for(int fy = 0; fy < FILTER_SY; fy++) {
        int oy0 = ay + fy - PAD;
        int oy1 = ax + fy - PAD;
        int oy2 = ay + fy - PAD + 30;
        int oy3 = ax + fy - PAD;
        for(int fx = 0; fx < FILTER_SX; fx++) {
          int ox0 = ax + fx - PAD;
          int ox1 = ay + fx - PAD;
          int ox2 = ax + fx - PAD;
          int ox3 = ay + fx - PAD + 30;

          calc_sum(f, V, fx, fy, ox0, oy0, &vector_sum0);
          calc_sum(f, V, fx, fy, ox1, oy1, &vector_sum1);
          calc_sum(f, V, fx, fy, ox2, oy2, &vector_sum2);
          calc_sum(f, V, fx, fy, ox3, oy3, &vector_sum3);
        }
      }
      // Since the initial depth is 3, only the first 3 values are being used in the array
      double value0 = sum0[0] + sum0[1] + sum0[2];
      double value1 = sum1[0] + sum1[1] + sum1[2];
      double value2 = sum2[0] + sum2[1] + sum2[2];
      double value3 = sum3[0] + sum3[1] + sum3[2];
      set_vol(A, ax, ay, d, value0 + l->biases->w[d]);
      set_vol(A, ay, ax, d, value1 + l->biases->w[d]);
      set_vol(A, ax, ay + 30, d, value2 + l->biases->w[d]);
      set_vol(A, ay + 30, ax, d, value3 + l->biases->w[d]);
    }
  }
}

// Optimized forward function for layer 0
void conv_forward_1(conv_layer_t* l, vol_t** in, vol_t** out, int start, int end) {
  for (int i = start; i <= end; i++) {
    vol_t* V = in[i];
    vol_t* A = out[i];

    for(int d = 0; d < L0_FILTERS; d++) {
      vol_t* f = l->filters[d];

      // In order to achieve a bigger loop unrolling, this part only processes the 2..30, 2..30 range
      // where the below condition is always true
      // oy >= 0 && oy < V_sy && ox >=0 && ox < V_sx
      for(int ay = 2; ay < 30; ay++) {
        for(int ax = 2; ax < 30; ax++) {
          // The initial dimensions (depth = 3 and fx = 5) allow to process 3 * 5 = 15 values at once
          __m256d vector_sum0 = _mm256_setzero_pd();
          __m256d vector_sum1 = _mm256_setzero_pd();
          __m256d vector_sum2 = _mm256_setzero_pd();
          __m256d vector_sum3 = _mm256_setzero_pd();
          // Declare arrays to easily access the contents of the vectors
          double*  sum0 = (double*) &vector_sum0;
          double*  sum1 = (double*) &vector_sum1;
          double*  sum2 = (double*) &vector_sum2;
          double*  sum3 = (double*) &vector_sum3;
          for(int fy = 0; fy < FILTER_SY; fy++) {
            int oy = ay + fy - PAD;
            int ox = ax - PAD;
            int f_start = ((FILTER_SX * fy)) * L0_DEPTH;
            int V_start = ((L0_SX * oy)+ox) * L0_DEPTH;
            // The actual computation with the intrinsic functions with a 4x4 unrolling
            __m256d multiplicand = _mm256_loadu_pd(f->w + f_start);
            __m256d multipllicator = _mm256_loadu_pd(V->w + V_start);
            __m256d product = _mm256_mul_pd(multiplicand, multipllicator);
            vector_sum0 = _mm256_add_pd(vector_sum0, product);

            multiplicand = _mm256_loadu_pd(f->w + f_start + 4);
            multipllicator = _mm256_loadu_pd(V->w + V_start + 4);
            product = _mm256_mul_pd(multiplicand, multipllicator);
            vector_sum1 = _mm256_add_pd(vector_sum1, product);

            multiplicand = _mm256_loadu_pd(f->w + f_start + 8);
            multipllicator = _mm256_loadu_pd(V->w + V_start + 8);
            product = _mm256_mul_pd(multiplicand, multipllicator);
            vector_sum2 = _mm256_add_pd(vector_sum2, product);

            multiplicand = _mm256_loadu_pd(f->w + f_start + 12);
            multipllicator = _mm256_loadu_pd(V->w + V_start + 12);
            product = _mm256_mul_pd(multiplicand, multipllicator);
            vector_sum3 = _mm256_add_pd(vector_sum3, product);
          }
          // Only 15 values are being processed so 4th value in the last array is ignored
          double value = sum0[0] + sum0[1] + sum0[2] + sum0[3];
          value += sum1[0] + sum1[1] + sum1[2] + sum1[3];
          value += sum2[0] + sum2[1] + sum2[2] + sum2[3];
          value += sum3[0] + sum3[1] + sum3[2];
          set_vol(A, ax, ay, d, value +  l->biases->w[d]);
            }
          }
      // This funciton handles the the part when the mentioned condition is not always true
      handle_corner_cases(f, V, A, l, d);
    }
  }
}

// Optimized forward function for layer 3
void conv_forward_2(conv_layer_t* l, vol_t** in, vol_t** out, int start, int end) {
  for (int i = start; i <= end; i++) {
    vol_t* V = in[i];
    vol_t* A = out[i];

    for(int d = 0; d < L3_FILTERS; d++) {
      vol_t* f = l->filters[d];
      for(int ay = 0; ay < L3_SY; ay++) {
        for(int ax = 0; ax < L3_SX; ax++) {
          __m256d vector_sum0 = _mm256_setzero_pd();
          __m256d vector_sum1 = _mm256_setzero_pd();
          __m256d vector_sum2 = _mm256_setzero_pd();
          __m256d vector_sum3 = _mm256_setzero_pd();
          double*  sum0 = (double*) &vector_sum0;
          double*  sum1 = (double*) &vector_sum1;
          double*  sum2 = (double*) &vector_sum2;
          double*  sum3 = (double*) &vector_sum3;
          for(int fy = 0; fy < FILTER_SY; fy++) {
            int oy = ay + fy - PAD;
            for(int fx = 0; fx < FILTER_SX; fx++) {
              int ox = ax + fx - PAD;
              // The actual computation with the intrinsic functions with a 4x4 unrolling since the current value for depth is 16
              // The optimization in forward_1 would work here as well, might implement it later
              if(oy >= 0 && oy < L3_SY && ox >=0 && ox < L3_SX) {
                int f_start = ((FILTER_SX * fy) + fx) * L3_DEPTH;
                int V_start = ((L3_SX * oy) + ox) * L3_DEPTH;

                __m256d multiplicand0 = _mm256_loadu_pd(f->w + f_start);
                __m256d multipllicator0 = _mm256_loadu_pd(V->w + V_start);
                __m256d product0 = _mm256_mul_pd(multiplicand0, multipllicator0);
                __m256d multiplicand1 = _mm256_loadu_pd(f->w + f_start + 4);
                __m256d multipllicator1 = _mm256_loadu_pd(V->w + V_start + 4);
                __m256d product1 = _mm256_mul_pd(multiplicand1, multipllicator1);
                __m256d multiplicand2 = _mm256_loadu_pd(f->w + f_start + 8);
                __m256d multipllicator2 = _mm256_loadu_pd(V->w + V_start + 8);
                __m256d product2 = _mm256_mul_pd(multiplicand2, multipllicator2);
                __m256d multiplicand3 = _mm256_loadu_pd(f->w + f_start + 12);
                __m256d multipllicator3 = _mm256_loadu_pd(V->w + V_start + 12);
                __m256d product3 = _mm256_mul_pd(multiplicand3, multipllicator3);

                vector_sum0 = _mm256_add_pd(vector_sum0, product0);
                vector_sum1 = _mm256_add_pd(vector_sum1, product1);
                vector_sum2 = _mm256_add_pd(vector_sum2, product2);
                vector_sum3 = _mm256_add_pd(vector_sum3, product3);
              }
            }
          }
          double value = sum0[0] + sum0[1] + sum0[2] + sum0[3];
          value += sum1[0] + sum1[1] + sum1[2] + sum1[3];
          value += sum2[0] + sum2[1] + sum2[2] + sum2[3];
          value += sum3[0] + sum3[1] + sum3[2] + sum3[3];
          set_vol(A, ax, ay, d, value + l->biases->w[d]);
        }
      }
    }
  }
}

// Optimized forward function for layer 6
void conv_forward_3(conv_layer_t* l, vol_t** in, vol_t** out, int start, int end) {
  for (int i = start; i <= end; i++) {
    vol_t* V = in[i];
    vol_t* A = out[i];

    for(int d = 0; d < L6_FILTERS; d++) {
      vol_t* f = l->filters[d];
      for(int ay = 0; ay < L6_SY; ay++) {
        for(int ax = 0; ax < L6_SY; ax++) {
          __m256d vector_sum0 = _mm256_setzero_pd();
          __m256d vector_sum1 = _mm256_setzero_pd();
          __m256d vector_sum2 = _mm256_setzero_pd();
          __m256d vector_sum3 = _mm256_setzero_pd();
          __m256d vector_sum4 = _mm256_setzero_pd();
          double*  sum0 = (double*) &vector_sum0;
          double*  sum1 = (double*) &vector_sum1;
          double*  sum2 = (double*) &vector_sum2;
          double*  sum3 = (double*) &vector_sum3;
          double*  sum4 = (double*) &vector_sum4;
          for(int fy = 0; fy < FILTER_SY; fy++) {
            int oy = ay + fy - PAD;
            for(int fx = 0; fx < FILTER_SX; fx++) {
              int ox = ax + fx - PAD;
              // The actual computation with the intrinsic functions with a 5x4 unrolling since the current value for depth is 20
              // The optimization in forward_1 would work here as well, might implement it later
              if(oy >= 0 && oy < L6_SY && ox >= 0 && ox < L6_SX) {
                int f_start = ((FILTER_SX * fy) + fx) * L6_DEPTH;
                int V_start = ((L6_SX * oy)+ ox) * L6_DEPTH;

                __m256d multiplicand0 = _mm256_loadu_pd(f->w + f_start);
                __m256d multipllicator0 = _mm256_loadu_pd(V->w + V_start);
                __m256d product0 = _mm256_mul_pd(multiplicand0, multipllicator0);
                __m256d multiplicand1 = _mm256_loadu_pd(f->w + f_start + 4);
                __m256d multipllicator1 = _mm256_loadu_pd(V->w + V_start + 4);
                __m256d product1 = _mm256_mul_pd(multiplicand1, multipllicator1);
                __m256d multiplicand2 = _mm256_loadu_pd(f->w + f_start + 8);
                __m256d multipllicator2 = _mm256_loadu_pd(V->w + V_start + 8);
                __m256d product2 = _mm256_mul_pd(multiplicand2, multipllicator2);
                __m256d multiplicand3 = _mm256_loadu_pd(f->w + f_start + 12);
                __m256d multipllicator3 = _mm256_loadu_pd(V->w + V_start + 12);
                __m256d product3 = _mm256_mul_pd(multiplicand3, multipllicator3);
                __m256d multiplicand4 = _mm256_loadu_pd(f->w + f_start + 16);
                __m256d multipllicator4 = _mm256_loadu_pd(V->w + V_start + 16);
                __m256d product4 = _mm256_mul_pd(multiplicand4, multipllicator4);

                vector_sum0 = _mm256_add_pd(vector_sum0, product0);
                vector_sum1 = _mm256_add_pd(vector_sum1, product1);
                vector_sum2 = _mm256_add_pd(vector_sum2, product2);
                vector_sum3 = _mm256_add_pd(vector_sum3, product3);
                vector_sum4 = _mm256_add_pd(vector_sum4, product4);
              }
            }
          }
          double value = sum0[0] + sum0[1] + sum0[2] + sum0[3];
          value += sum1[0] + sum1[1] + sum1[2] + sum1[3];
          value += sum2[0] + sum2[1] + sum2[2] + sum2[3];
          value += sum3[0] + sum3[1] + sum3[2] + sum3[3];
          value += sum4[0] + sum4[1] + sum4[2] + sum4[3];
          set_vol(A, ax, ay, d, value + l->biases->w[d]);
        }
      }
    }
  }
}

void conv_load(conv_layer_t* l, const char* fn) {
  int sx, sy, depth, filters;

  FILE* fin = fopen(fn, "r");

  fscanf(fin, "%d %d %d %d", &sx, &sy, &depth, &filters);
  assert(sx == l->sx);
  assert(sy == l->sy);
  assert(depth == l->in_depth);
  assert(filters == l->out_depth);

  for(int d = 0; d < l->out_depth; d++)
    for (int x = 0; x < sx; x++)
      for (int y = 0; y < sy; y++)
        for (int z = 0; z < depth; z++) {
          double val;
          fscanf(fin, "%lf", &val);
          set_vol(l->filters[d], x, y, z, val);
        }

  for(int d = 0; d < l->out_depth; d++) {
    double val;
    fscanf(fin, "%lf", &val);
    set_vol(l->biases, 0, 0, d, val);
  }

  fclose(fin);
}

// Relu Layer -----------------------------------------------------------------

typedef struct relu_layer {
  // required
  int in_depth;
  int in_sx;
  int in_sy;

  // computed
  int out_depth;
  int out_sx;
  int out_sy;
} relu_layer_t;

relu_layer_t* make_relu_layer(int in_sx, int in_sy, int in_depth) {
  relu_layer_t* l = (relu_layer_t*)malloc(sizeof(relu_layer_t));

  // required
  l->in_depth = in_depth;
  l->in_sx = in_sx;
  l->in_sy = in_sy;

  // computed
  l->out_sx = l->in_sx;
  l->out_sy = l->in_sy;
  l->out_depth = l->in_depth;

  return l;
}

void relu_forward(relu_layer_t* l, vol_t** in, vol_t** out, int start, int end) {
  for (int j = start; j <= end; j++) {
    for (int i = 0; i < l->in_sx*l->in_sy*l->in_depth; i++) {
      out[j]->w[i] = (in[j]->w[i] < 0.0) ? 0.0 : in[j]->w[i];
    }
  }
}

// Pool Layer -----------------------------------------------------------------

typedef struct pool_layer {
  // required
  int sx;
  int in_depth;
  int in_sx;
  int in_sy;

  // optional
  int sy;
  int stride;
  int pad;

  // computed
  int out_depth;
  int out_sx;
  int out_sy;
} pool_layer_t;

pool_layer_t* make_pool_layer(int in_sx, int in_sy, int in_depth,
                              int sx, int stride) {
  pool_layer_t* l = (pool_layer_t*)malloc(sizeof(pool_layer_t));

  // required
  l->sx = sx;
  l->in_depth = in_depth;
  l->in_sx = in_sx;
  l->in_sy = in_sy;

  // optional
  l->sy = l->sx;
  l->stride = stride;
  l->pad = 0;

  // computed
  l->out_depth = in_depth;
  l->out_sx = floor((l->in_sx + l->pad * 2 - l->sx) / l->stride + 1);
  l->out_sy = floor((l->in_sy + l->pad * 2 - l->sy) / l->stride + 1);

  return l;
}

void pool_forward(pool_layer_t* l, vol_t** in, vol_t** out, int start, int end) {
  for (int i = start; i <= end; i++) {
    vol_t* V = in[i];
    vol_t* A = out[i];
        
    int n=0;
    for(int d=0;d<l->out_depth;d++) {
      int x = -l->pad;
      int y = -l->pad;
      for(int ax=0; ax<l->out_sx; x+=l->stride,ax++) {
        y = -l->pad;
        for(int ay=0; ay<l->out_sy; y+=l->stride,ay++) {
  
          double a = -99999;
          for(int fx=0;fx<l->sx;fx++) {
            for(int fy=0;fy<l->sy;fy++) {
              int oy = y+fy;
              int ox = x+fx;
              if(oy>=0 && oy<V->sy && ox>=0 && ox<V->sx) {
                double v = get_vol(V, ox, oy, d);
                if(v > a) { a = v; }
              }
            }
          }
          n++;
          set_vol(A, ax, ay, d, a);
        }
      }
    }
  }
}

// FC Layer -------------------------------------------------------------------

typedef struct fc_layer {
  // required
  int out_depth;
  int in_depth;
  int in_sx;
  int in_sy;

  // optional
  double l1_decay_mul;
  double l2_decay_mul;

  // computed
  int out_sx;
  int out_sy;
  int num_inputs;
  double bias;
  vol_t* biases;
  vol_t** filters;
} fc_layer_t;

fc_layer_t* make_fc_layer(int in_sx, int in_sy, int in_depth,
                          int num_neurons) {
  fc_layer_t* l = (fc_layer_t*)malloc(sizeof(fc_layer_t));

  // required
  l->out_depth = num_neurons;
  l->in_depth = in_depth;
  l->in_sx = in_sx;
  l->in_sy = in_sy;
    
  // optional
  l->l1_decay_mul = 0.0;
  l->l2_decay_mul = 1.0;

  // computed
  l->num_inputs = l->in_sx * l->in_sy * l->in_depth;
  l->out_sx = 1;
  l->out_sy = 1;

  l->filters = (vol_t**)malloc(sizeof(vol_t*)*num_neurons);
  for (int i = 0; i < l->out_depth; i++) {
    l->filters[i] = make_vol(1, 1, l->num_inputs, 0.0);
  }

  l->bias = 0.0;
  l->biases = make_vol(1, 1, l->out_depth, l->bias);

  return l;
}

void fc_forward(fc_layer_t* l, vol_t** in, vol_t** out, int start, int end) {
  for (int j = start; j <= end; j++) {
    vol_t* V = in[j];
    vol_t* A = out[j];
        
    for(int i=0;i<l->out_depth;i++) {
      double a = 0.0;
      for(int d=0;d<l->num_inputs;d++) {
        a += V->w[d] * l->filters[i]->w[d];
      }
      a += l->biases->w[i];
      A->w[i] = a;
    }
  }
}

void fc_load(fc_layer_t* l, const char* fn) {
  FILE* fin = fopen(fn, "r");

  int num_inputs;
  int out_depth;
  fscanf(fin, "%d %d", &num_inputs, &out_depth);
  assert(out_depth == l->out_depth);
  assert(num_inputs == l->num_inputs);

  for(int i = 0; i < l->out_depth; i++)
    for(int d = 0; d < l->num_inputs; d++) {
      double val;
      fscanf(fin, "%lf", &val);
      l->filters[i]->w[d] = val;
    }

  for(int i = 0; i < l->out_depth; i++) {
    double val;
    fscanf(fin, "%lf", &val);
    l->biases->w[i] = val;
  }

  fclose(fin);
}

// Softmax Layer --------------------------------------------------------------

// Maximum supported out_depth
#define MAX_ES 16

typedef struct softmax_layer {
  // required
  int in_depth;
  int in_sx;
  int in_sy;
  double* es; 

  // computed
  int out_depth;
  int out_sx;
  int out_sy;
} softmax_layer_t;

softmax_layer_t* make_softmax_layer(int in_sx, int in_sy, int in_depth) {
  softmax_layer_t* l = (softmax_layer_t*)malloc(sizeof(softmax_layer_t));

  // required
  l->in_depth = in_depth;
  l->in_sx = in_sx;
  l->in_sy = in_sy;

  // computed
  l->out_sx = 1;
  l->out_sy = 1;
  l->out_depth = l->in_sx * l->in_sy * l->in_depth;

  l->es = (double*)malloc(sizeof(double)*l->out_depth);

  return l;
}

void softmax_forward(softmax_layer_t* l, vol_t** in, vol_t** out, int start, int end) {
  double es[MAX_ES];

  for (int j = start; j <= end; j++) {
    vol_t* V = in[j];
    vol_t* A = out[j];
  
    // compute max activation
    double amax = V->w[0];
    for(int i=1;i<l->out_depth;i++) {
      if(V->w[i] > amax) amax = V->w[i];
    }
  
    // compute exponentials (carefully to not blow up)
    double esum = 0.0;
    for(int i=0;i<l->out_depth;i++) {
      double e = exp(V->w[i] - amax);
      esum += e;
      es[i] = e;
    }
  
    // normalize and output to sum to one
    for(int i=0;i<l->out_depth;i++) {
      es[i] /= esum;
      A->w[i] = es[i];
    }
  }
}

// Neural Network -------------------------------------------------------------

/*
 * This represents the CNN we will use in this project. It consists of 11
 * layers, which means that there are 12 volumes of data (where the first one
 * is the input data and the last one the classification result).
 */

#define LAYERS 11

typedef struct network {
  vol_t* v[LAYERS+1];
  conv_layer_t* l0;
  relu_layer_t* l1;
  pool_layer_t* l2;
  conv_layer_t* l3;
  relu_layer_t* l4;
  pool_layer_t* l5;
  conv_layer_t* l6;
  relu_layer_t* l7;
  pool_layer_t* l8;
  fc_layer_t* l9;
  softmax_layer_t* l10;
} network_t;

/*
 * Instantiate our specific CNN.
 */

network_t* make_network() {
  network_t* net = (network_t*)malloc(sizeof(network_t));
  net->v[0] = make_vol(L0_SX, L0_SY, L0_DEPTH, 0.0);
  net->l0 = make_conv_layer(L0_SX, L0_SY, L0_DEPTH, FILTER_SX, L0_FILTERS, STRIDE, PAD);
  net->v[1] = make_vol(net->l0->out_sx, net->l0->out_sy, net->l0->out_depth, 0.0);
  net->l1 = make_relu_layer(net->v[1]->sx, net->v[1]->sy, net->v[1]->depth);
  net->v[2] = make_vol(net->l1->out_sx, net->l1->out_sy, net->l1->out_depth, 0.0);
  net->l2 = make_pool_layer(net->v[2]->sx, net->v[2]->sy, net->v[2]->depth, 2, 2);
  net->v[3] = make_vol(net->l2->out_sx, net->l2->out_sy, net->l2->out_depth, 0.0);
  net->l3 = make_conv_layer(net->v[3]->sx, net->v[3]->sy, net->v[3]->depth, FILTER_SX, L3_FILTERS, STRIDE, PAD);
  net->v[4] = make_vol(net->l3->out_sx, net->l3->out_sy, net->l3->out_depth, 0.0);
  net->l4 = make_relu_layer(net->v[4]->sx, net->v[4]->sy, net->v[4]->depth);
  net->v[5] = make_vol(net->l4->out_sx, net->l4->out_sy, net->l4->out_depth, 0.0);
  net->l5 = make_pool_layer(net->v[5]->sx, net->v[5]->sy, net->v[5]->depth, 2, 2);
  net->v[6] = make_vol(net->l5->out_sx, net->l5->out_sy, net->l5->out_depth, 0.0);
  net->l6 = make_conv_layer(net->v[6]->sx, net->v[6]->sy, net->v[6]->depth, FILTER_SX, L6_FILTERS, STRIDE, PAD);
  net->v[7] = make_vol(net->l6->out_sx, net->l6->out_sy, net->l6->out_depth, 0.0);
  net->l7 = make_relu_layer(net->v[7]->sx, net->v[7]->sy, net->v[7]->depth);
  net->v[8] = make_vol(net->l7->out_sx, net->l7->out_sy, net->l7->out_depth, 0.0);
  net->l8 = make_pool_layer(net->v[8]->sx, net->v[8]->sy, net->v[8]->depth, 2, 2);
  net->v[9] = make_vol(net->l8->out_sx, net->l8->out_sy, net->l8->out_depth, 0.0);
  net->l9 = make_fc_layer(net->v[9]->sx, net->v[9]->sy, net->v[9]->depth, 10);
  net->v[10] = make_vol(net->l9->out_sx, net->l9->out_sy, net->l9->out_depth, 0.0);
  net->l10 = make_softmax_layer(net->v[10]->sx, net->v[10]->sy, net->v[10]->depth);
  net->v[11] = make_vol(net->l10->out_sx, net->l10->out_sy, net->l10->out_depth, 0.0);
  return net;
}

/* 
 * Free our specific CNN.
 */

void free_network(network_t* net) {
  for (int i = 0; i < LAYERS+1; i++)
    free_vol(net->v[i]);

  free(net->l0);
  free(net->l1);
  free(net->l2);
  free(net->l3);
  free(net->l4);
  free(net->l5);
  free(net->l6);
  free(net->l7);
  free(net->l8);
  free(net->l9);
  free(net->l10);

  free(net);
}

/*
 * We organize data as "batches" of volumes. Each batch consists of a number of samples,
 * each of which contains a volume for every intermediate layer. Say we have L layers
 * and a set of N input images. Then batch[l][n] contains the volume at layer l for
 * input image n.
 *
 * By using batches, we can process multiple images at once in each run of the forward
 * functions of the different layers.
 */

typedef vol_t** batch_t;

/*
 * This function allocates a new batch for the network old_net with size images.
 */

batch_t* make_batch(network_t* old_net, int size) {
  batch_t* out = (batch_t*)malloc(sizeof(vol_t**)*(LAYERS+1));
  for (int i = 0; i < LAYERS+1; i++) {
    out[i] = (vol_t**)malloc(sizeof(vol_t*)*size);
    for (int j = 0; j < size; j++) {
      out[i][j] = make_vol(old_net->v[i]->sx, old_net->v[i]->sy, old_net->v[i]->depth, 0.0);
    }
  }

  return out;
}

/*
 * Free a previously allocated batch.
 */

void free_batch(batch_t* v, int size) {
  for (int i = 0; i < LAYERS+1; i++) {
    for (int j = 0; j < size; j++) {
      free_vol(v[i][j]);
    }
    free(v[i]);
  }
  free(v);
}

/*
 * Apply our network to a specific batch of inputs. The batch has to be given
 * as input to v and start/end are the first and the last image in that batch
 * to process (start and end are inclusive).
 */
void net_forward(network_t* net, batch_t* v, int start, int end) {
  conv_forward_1(net->l0, v[0], v[1], start, end);
  relu_forward(net->l1, v[1], v[2], start, end);
  pool_forward(net->l2, v[2], v[3], start, end);
  conv_forward_2(net->l3, v[3], v[4], start, end);
  relu_forward(net->l4, v[4], v[5], start, end);
  pool_forward(net->l5, v[5], v[6], start, end);
  conv_forward_3(net->l6, v[6], v[7], start, end);
  relu_forward(net->l7, v[7], v[8], start, end);
  pool_forward(net->l8, v[8], v[9], start, end);
  fc_forward(net->l9, v[9], v[10], start, end);
  softmax_forward(net->l10, v[10], v[11], start, end);
}

/*
 * Putting everything together: Take a set of n input images as 3-dimensional
 * Volumes and process them using the CNN in batches of 1. Then look at the
 * output (which is a set of 10 labels, each of which tells us the likelihood
 * of a specific category) and classify the image as a cat iff the likelihood
 * of "cat" is larger than 50%. Writes the cat likelihood for all images into
 * an output array (0 = definitely no cat, 1 = definitely cat).
 */
#define CAT_LABEL 3
void net_classify_cats(network_t* net, vol_t** input, double* output, int n) {
  int max_threads = omp_get_max_threads();
  int num_threads = n < max_threads ? n : max_threads;
  int batch_size = n / num_threads;

    #pragma omp parallel num_threads(num_threads)
	{
      int tid = omp_get_thread_num();
      batch_t* batch = make_batch(net, batch_size);
      int start = tid * batch_size;

      for (int i = 0; i < batch_size; i++) {
        copy_vol(batch[0][i], input[start + i]);
      }

      net_forward(net, batch, 0, batch_size - 1);

      for (int i = 0; i < batch_size; i++) {
        output[start + i] = batch[11][i]->w[CAT_LABEL];
      }

      free_batch(batch, batch_size);
    }

  int lastProcessed = batch_size * num_threads;
  int leftovers = n - lastProcessed;
  if (leftovers) {
    #pragma omp parallel num_threads(leftovers)
    {
      int tid = omp_get_thread_num();
      batch_t* batch = make_batch(net, 1);
      copy_vol(batch[0][0], input[lastProcessed + tid]);
      net_forward(net, batch, 0, 0);
      output[lastProcessed + tid] = batch[11][0]->w[CAT_LABEL];
      free_batch(batch, 1);
    }
  }
}

// IGNORE EVERYTHING BELOW THIS POINT -----------------------------------------

// Including C files in other C files is very bad style and should be avoided
// in any real application. We do it here since we want everything that you
// may edit to be in one file, without having to fix the interfaces between
// the different components of the system.

#include "util.c"
#include "main.c"
