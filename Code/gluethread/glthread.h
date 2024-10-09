#ifndef __GLUETHREAD__
#define __GLUETHREAD__

typedef struct _glthread
{
	struct glthread* left ;
	struct glthread* right ;
} glthread_t ;

void glthread_add_next ( glthread_t* base_glthread , glthread_t* new_glthread );

void glthread_add_before ( glthread_t* base_glthread , glthread_t* new_glthread );

void remove_glthread ( glthread_t* glthread );

void init_glthread ( glthread_t * glthread );

void glthread_add_last ( glthread_t* base_glthread , glthread_t* new_glthread );


#endif
