// 1. In which memory sections (CODE, STATIC, HEAP, STACK) do the following reside?

#define C 2
const int val = 16;
char arr[] = "foo";
void foo(int arg){
	char *str = (char *) malloc (C*val);
	char *ptr = arr;
}

//	arg [STACK] 	str [STACK]
//	arr [STATIC] 	*str[HEAP]
//	val [CODE]	 	C	[CODE]

// 2. What is wrong with the C code below?

int* ptr = malloc(4 * sizeof(int));
if(extra_large) ptr = malloc(10 * sizeof(int));
return ptr;

// If extra_large evalutes to true then memory leak occurs (there is no corresponding call to free for the first malloc call),
// the pointer to that chunk of memory is lost.

// 3. Write code to prepend (add to the start) to a linked list, and to free/empty the entire list.
// struct ll_node { struct ll_node* next; int value; }

void free_ll(struct ll_node** list) {
	if (*list) {
		free_ll(&((*list)->next));
		free(*list);
	}
	*list = NULL;
}

void prepend(struct ll_node** list, int value) {
	struct ll_node* new_node = malloc(sizeof(struct ll_node));
	new_node->value = value;
	new_node->next = *list;
	*list = new_node;
}
