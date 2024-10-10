#ifndef __UAPI_MM__
#define __UAPI_MM__

#include<stdint.h>

void* xcalloc(char* struct_name , int units);

#define XCALLOC(units , struct_name ) \
	(xcalloc(#struct_name , units))

// initialization functions
void mm_init();

// registration function 
void mm_instantiate_new_page_family
(char* struct_name,uint32_t struct_size);

void mm_print_memory_usage(char* struct_name);
void mm_print_registered_page_families();
void mm_print_block_usage() ;

#define MM_REG_STRUCT(struct_name) \
	(mm_instantiate_new_page_family(#struct_name, sizeof(struct_name)))

#endif /* __UAPI_MM__ */

