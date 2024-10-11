#include<stdio.h>
#include<memory.h>
#include<unistd.h>  // for geting page size
#include<sys/mman.h> // for using mmap()
#include<stdint.h>
#include"mm.h"
#include"css.h"
#include<assert.h>

static vm_page_for_families_t* first_vm_page_for_families = NULL ;// called in line 414 check if error
static size_t SYSTEM_PAGE_SIZE = 0 ;
void *gb_hsba = NULL ; // heap segment start for block allocation



void mm_init()
{
	SYSTEM_PAGE_SIZE = getpagesize();

}



static void* mm_get_new_vm_page_from_kernel(int size)
{
	char* vm_page = mmap(
			0,
			size*SYSTEM_PAGE_SIZE,
			PROT_READ|PROT_WRITE|PROT_EXEC,
			MAP_ANON | MAP_PRIVATE,
			0 , 0 );
	if (vm_page == MAP_FAILED)
	{
		printf("Error: Virutal Memory Page Allocation Failed !");
		return NULL ;
	}
	memset(vm_page , 0 , size * SYSTEM_PAGE_SIZE);
	return (void*) vm_page ;
}



static void return_vm_to_kernel(void* vm_page , int size)
{
	if(munmap(vm_page , size * SYSTEM_PAGE_SIZE))
	{
		printf("Error: could not unmap vm_page to kernel");
	};

}



void mm_instantiate_new_page_family( char* struct_name , uint32_t struct_size  )
{
	vm_page_family_t* vm_page_family_curr = NULL ;
	vm_page_for_families_t* new_vm_page_for_families = NULL ;
	if (struct_size > SYSTEM_PAGE_SIZE )
	{
		printf("Error: %s Structure %s Size exceeds System page size \n.",
		__FUNCTION__ , struct_name );
		return ;
	}
	if (! first_vm_page_for_families)
	{
		first_vm_page_for_families = (vm_page_for_families_t*) mm_get_new_vm_page_from_kernel(1);
		first_vm_page_for_families->next = NULL ;
		strncpy(first_vm_page_for_families->vm_page_family[0].struct_name , struct_name , MM_MAX_STRUCT_NAME );
		first_vm_page_for_families->vm_page_family[0].struct_size = struct_size ;
		first_vm_page_for_families->vm_page_family[0].first_page = NULL ;
		init_glthread(&first_vm_page_for_families->vm_page_family[0].free_block_priority_list_head);
		return ;
	}
	uint32_t count = 0 ;
	ITERATE_PAGE_FAMILIES_BEGIN(first_vm_page_for_families , vm_page_family_curr )
	{
		if( strncmp(vm_page_family_curr->struct_name,struct_name,MM_MAX_STRUCT_NAME) != 0 )
		{
			count++;
			continue;
		}
		assert(0);
	} ITERATE_PAGE_FAMILIES_END(first_vm_page_for_families,vm_page_family_curr );
	if (count == MAX_FAMILIES_PER_VM_PAGE)
	{
		new_vm_page_for_families = (vm_page_for_families_t*) mm_get_new_vm_page_from_kernel(1) ;
		new_vm_page_for_families->next = first_vm_page_for_families ;
		first_vm_page_for_families = new_vm_page_for_families ;
		//vm_page_family_curr = &first_vm_page_for_families->vm_page_family[0];
	}

	strncpy(vm_page_family_curr->struct_name , struct_name , MM_MAX_STRUCT_NAME );
	vm_page_family_curr->struct_size = struct_size ;
	vm_page_family_curr->first_page = NULL ;
	init_glthread(&vm_page_family_curr->free_block_priority_list_head);
}



void mm_print_registered_page_families()
{
	vm_page_family_t* vm_page_family_curr = NULL ;
	vm_page_for_families_t* vm_page_for_families_curr = NULL ;
	for( vm_page_for_families_curr = first_vm_page_for_families ;
	vm_page_for_families_curr ;
	vm_page_for_families_curr = vm_page_for_families_curr->next )
	{
		ITERATE_PAGE_FAMILIES_BEGIN(vm_page_for_families_curr , vm_page_family_curr )
		{
			printf("Page Family : %s , Size : %u \n",
			vm_page_family_curr->struct_name , 
			vm_page_family_curr->struct_size );
		} ITERATE_PAGE_FAMILIES_END(vm_page_for_families_curr , vm_page_family_curr );
	}
}


vm_page_family_t* lookup_page_family_by_name(char* struct_name)
{
	vm_page_family_t * vm_page_family_curr = NULL ;
	vm_page_for_families_t* vm_page_for_families_curr = NULL ;
	for ( vm_page_for_families_curr = first_vm_page_for_families ;
	vm_page_for_families_curr ; 
	vm_page_for_families_curr = vm_page_for_families_curr->next )
	{
		ITERATE_PAGE_FAMILIES_BEGIN(first_vm_page_for_families , vm_page_family_curr )
		{
			if ( strncmp(vm_page_family_curr->struct_name , struct_name , MM_MAX_STRUCT_NAME ) == 0 )
			{
				return vm_page_family_curr ;
			}
		} ITERATE_PAGE_FAMILIES_END( first_vm_page_for_families , vm_page_family_curr );
	}
	return NULL ;
}


static void mm_union_free_blocks(block_meta_data_t* first , block_meta_data_t* second )
{
	assert( first->is_free == MM_TRUE && second->is_free == MM_TRUE );
	first->block_size += sizeof(block_meta_data_t) + second->block_size ;
	first->next_block = second->next_block ;
	if (second->next_block)
	{
		second->next_block->prev_block = first ;
	}
}


vm_bool_t mm_is_vm_page_empty(vm_page_t* vm_page) 
{
	if( vm_page->block_meta_data.next_block == NULL &&
	vm_page->block_meta_data.prev_block == NULL &&
	vm_page->block_meta_data.is_free == MM_TRUE )
	{
		return MM_TRUE ;
	}
	return MM_FALSE ;
}

static inline uint32_t mm_max_page_allocatable_memory ( int units )
{
	return (uint32_t) 
	((SYSTEM_PAGE_SIZE * units ) - offset_of(vm_page_t , page_memory ));
}

vm_page_t* allocate_vm_page ( vm_page_family_t* vm_page_family )
{
	vm_page_t * vm_page = mm_get_new_vm_page_from_kernel(1) ;
	/*Initialize lower most meta block of the vm page */
	MARK_VM_PAGE_EMPTY(vm_page) ;
	vm_page->block_meta_data.block_size = mm_max_page_allocatable_memory(1);
	vm_page->block_meta_data.offset = offset_of(vm_page_t , block_meta_data) ;
	init_glthread(&vm_page->block_meta_data.priority_thread_glue);
	vm_page->next = NULL ;
	vm_page->prev = NULL ;
	/*set the back pointer to page family */
	vm_page->pg_family = vm_page_family ;
	/* if it is a first VM page for a given page family */
	if(!vm_page_family->first_page)
	{
		vm_page_family->first_page = vm_page ;
		return vm_page ;
	}
	/* Insert new VM page to the head of the linked list*/
	vm_page->next = vm_page_family->first_page ;
	vm_page_family->first_page->prev = vm_page ;
	vm_page_family->first_page = vm_page ;
	return vm_page ;
}


void mm_vm_page_delete_and_free(vm_page_t* vm_page )
{
	vm_page_family_t* vm_page_family = vm_page->pg_family ;
	/* If the page being deletes is the head of the linked list */
	if(vm_page_family->first_page == vm_page )
	{
		vm_page_family->first_page = vm_page->next ;
		if ( vm_page->next )
		{
			vm_page->next->prev = NULL ;
		}
		vm_page->next = NULL ;
		vm_page->prev = NULL ;
		return_vm_to_kernel((void*)vm_page , 1 );
		return ;
	}
	/* if we are deleting the page from the middle or end of the linked list*/
	if(vm_page->next)
	{
		vm_page->next->prev = vm_page->prev ;
	}
	vm_page->prev->next = vm_page->next ;
	return_vm_to_kernel((void*) vm_page , 1 );
}


static int free_blocks_comparison_function( void* _block_meta_data1 , void* _block_meta_data2 )
{
	block_meta_data_t* block_meta_data1 = (block_meta_data_t* ) _block_meta_data1 ;
	block_meta_data_t* block_meta_data2 = (block_meta_data_t* ) _block_meta_data2 ;
	if( block_meta_data1->block_size > block_meta_data2->block_size )
	{
		return -1 ;
	}
	else if ( block_meta_data1->block_size < block_meta_data2->block_size )
	{
		return 1 ;
	}
	return 0 ;
}

static void mm_add_free_block_meta_data_to_free_block_list(
	vm_page_family_t* vm_page_family , block_meta_data_t* free_block )
{
	assert(free_block->is_free == MM_TRUE );
	glthread_priority_insert(&vm_page_family->free_block_priority_list_head ,
	&free_block->priority_thread_glue , free_blocks_comparison_function , 
	offset_of( block_meta_data_t , priority_thread_glue )) ;
}

//remove_glthread(&block_meta_data_t->priority_thread_glue);
//IS_GLTHREAD_LIST_EMPTY(&block_meta_data_t->priority_thread_glue);


static vm_page_t* mm_family_new_page_add( vm_page_family_t* vm_page_family )
{
	vm_page_t* vm_page = allocate_vm_page(vm_page_family);
	if( !vm_page )
	{
		return NULL ;
	}
	// Add the new page to free block list 
	mm_add_free_block_meta_data_to_free_block_list(vm_page_family , &vm_page->block_meta_data );
	return vm_page ;
}


static vm_bool_t mm_split_free_data_block_for_allocation(
			vm_page_family_t* vm_page_family , block_meta_data_t* block_meta_data ,
					uint32_t size )
{
	block_meta_data_t* next_block_meta_data = NULL ;
	assert(block_meta_data->is_free == MM_TRUE);
	if( block_meta_data->block_size < size )
	{
		return MM_FALSE ;
	}

	uint32_t remaining_size = block_meta_data->block_size - size ;
	block_meta_data->is_free = MM_FALSE ;
	block_meta_data->block_size = size ;
	remove_glthread(&block_meta_data->priority_thread_glue);
	// block_meta_data->offset = ??

	// CASE1 : NO SPLIT 
	if (! remaining_size )
	{
		return MM_TRUE ;
	}
	// CASE3 : PARTIAL SPLIT SOFT IF
	else if ( sizeof(block_meta_data_t) < remaining_size && 
			remaining_size < ( sizeof(block_meta_data_t) + vm_page_family->struct_size ))
	{
		// new meta block to be created 
		next_block_meta_data = NEXT_META_BLOCK_BY_SIZE(block_meta_data);
		next_block_meta_data->is_free = MM_TRUE ;
		next_block_meta_data->block_size = remaining_size - sizeof(block_meta_data_t);
		next_block_meta_data->offset = block_meta_data->offset + 
				sizeof(block_meta_data_t) + block_meta_data->block_size ;
		init_glthread(&next_block_meta_data->priority_thread_glue);
		mm_add_free_block_meta_data_to_free_block_list( vm_page_family , next_block_meta_data );
		mm_bind_blocks_for_allocation(block_meta_data , next_block_meta_data );
	}
	// CASE3 : PARTIAL SPLIT HARD IF 
	else if (remaining_size < sizeof(block_meta_data_t))
	{
		// no need to do anything
	}
	// CASE2 : FULL SPLIT NEW META BLOCK CREATED
	else
	{
		// new meta block to be created 
		next_block_meta_data = NEXT_META_BLOCK_BY_SIZE(block_meta_data);
		next_block_meta_data->is_free = MM_TRUE ;
		next_block_meta_data->block_size = remaining_size - sizeof(block_meta_data_t);
		next_block_meta_data->offset = block_meta_data->offset + 
				sizeof(block_meta_data_t) + block_meta_data->block_size ;
		init_glthread(&next_block_meta_data->priority_thread_glue);
		mm_add_free_block_meta_data_to_free_block_list( vm_page_family , next_block_meta_data );
		mm_bind_blocks_for_allocation(block_meta_data , next_block_meta_data );
	}
	return MM_TRUE ;
}



static block_meta_data_t* mm_allocate_free_data_block( 
			vm_page_family_t* vm_page_family  ,  uint32_t req_size )
{
	vm_bool_t status = MM_FALSE ;
	vm_page_t* vm_page = NULL ;
	block_meta_data_t* block_meta_data = NULL ;

	block_meta_data_t *biggest_block_meta_data = 
		mm_get_biggest_free_block_page_family(vm_page_family);
	if ( !biggest_block_meta_data || 
				biggest_block_meta_data->block_size < req_size )
	{
		// add new page to staisfy the request
		vm_page = mm_family_new_page_add(vm_page_family);
		// allocate free block from this page now 
		status = mm_split_free_data_block_for_allocation(vm_page_family, 
						&vm_page->block_meta_data , req_size );
		if ( status )
		{
			return &vm_page->block_meta_data;
		}
		return NULL ;
	}
	// The biggest block meta data can satisfy the request
	if (biggest_block_meta_data)
	{
		status = mm_split_free_data_block_for_allocation(vm_page_family , 
				biggest_block_meta_data , req_size );
	}
	if( status )
	{
		return biggest_block_meta_data ;
	}
	return NULL ;
	
}

/*function for dynamic memory allocation*/
void* xcalloc( char* struct_name , int units )
{
	// step1
	vm_page_family_t* pg_family = lookup_page_family_by_name(struct_name);
	if(! pg_family )
	{
		printf("Error: Structure %s not registered with Memory Manager\n", struct_name );
		return NULL ;
	}

	if( units * pg_family->struct_size > mm_max_page_allocatable_memory(1) )
	{
		printf("Error: Memory requested exceeds page size \n");
		return NULL ;
	}
	// find page which can satisfy the request
	block_meta_data_t * free_block_meta_data = NULL ;
	free_block_meta_data = mm_allocate_free_data_block( pg_family ,units * pg_family->struct_size );//function not defined
	if (free_block_meta_data)
	{
		memset((char*)(free_block_meta_data + 1) ,0 , free_block_meta_data->block_size );
		return (void*)(free_block_meta_data + 1);
	}
	return NULL ;
}


void mm_print_vm_page_details( vm_page_t* vm_page , uint32_t i )
{
	printf("\t Page Index : %u , address = %p\n" , vm_page->page_index , vm_page );
	printf("\t\t next = %p , prev = %p \n", vm_page->next , vm_page->prev );
	printf("\t\t page family = %s \n " ,  vm_page->pg_family->struct_name );

	uint32_t j = 0 ;
	block_meta_data_t* curr ;
	ITERATE_VM_PAGE_ALL_BLOCKS_BEGIN(vm_page , curr )
	{

		printf( ANSI_COLOR_YELLOW "\t\t\t%-14p block %-3u %s block_size = %-6u  " 
				"offset = %-6u prev = %-14p next = %p \n" ANSI_COLOR_RESET , curr ,
				j++ , curr->is_free ? "F R E E D" : "ALLOCATED" , curr->block_size ,
				curr->offset , curr->prev_block , curr->next_block );

	} ITERATE_VM_PAGE_ALL_BLOCKS_END( vm_page , curr );
}

void mm_print_memory_usage( char* struct_name )
{
	uint32_t i = 0 ;
	vm_page_t *vm_page = NULL ;
	vm_page_family_t *vm_page_family_curr ;
	uint32_t number_of_struct_families = 0 ;
	uint32_t total_memory_in_use_by_application = 0 ;
	uint32_t cumulative_vm_pages_claimed_from_kernel = 0 ;

	printf("\n vm page size = %zu bytes \n", SYSTEM_PAGE_SIZE );

	ITERATE_PAGE_FAMILIES_BEGIN( first_vm_page_for_families , vm_page_family_curr ){

		if (struct_name)
		{
			if(strncmp(struct_name , vm_page_family_curr->struct_name , 
						strlen(vm_page_family_curr->struct_name)))
			{
				continue;
			}
		}
		number_of_struct_families++ ;
		
		// definition of ANSI_COLOR_(NAME OF COLOR) GIVEN IN css.h
		printf(ANSI_COLOR_GREEN "vm_page_family : %s, struct size : %u \n" 
				ANSI_COLOR_RESET , vm_page_family_curr->struct_name ,
				vm_page_family_curr->struct_size );
		
		printf(ANSI_COLOR_CYAN "\tapp used memory %uB , #sys calls %u \n" 
				ANSI_COLOR_RESET , vm_page_family_curr->total_memory_in_use_by_app ,
				vm_page_family_curr->no_of_system_calls_to_alloc_dealloc_vm_pages );
		
		total_memory_in_use_by_application += vm_page_family_curr->total_memory_in_use_by_app ;
		i =  0 ;

		ITERATE_VM_PAGE_PER_FAMILY_BEGIN( vm_page_family_curr , vm_page )
		{

			cumulative_vm_pages_claimed_from_kernel++;
			mm_print_vm_page_details( vm_page , i++ ) ;

		} ITERATE_VM_PAGE_PER_FAMILY_END( vm_page_family_curr , vm_page );

	} ITERATE_PAGE_FAMILIES_END( first_vm_page_for_families , vm_page_family_curr );

	printf( ANSI_COLOR_MAGENTA "\n total application memory usage : %u bytes \n" ANSI_COLOR_RESET ,
			total_memory_in_use_by_application ) ;
	
	printf( ANSI_COLOR_MAGENTA "# of vm pages in use : %u (%lu bytes).\n" \
			"heap segment start ptr = %p , sbrk(0) = %p , gb_hsba = %p , diff = %lu  \n" \
			ANSI_COLOR_RESET , cumulative_vm_pages_claimed_from_kernel , first_vm_page_for_families
			, sbrk(0) , gb_hsba , (unsigned long)sbrk(0) - (unsigned long)gb_hsba );

	float memory_app_use_to_total_memory_ratio = 0.0 ;
	
	if( cumulative_vm_pages_claimed_from_kernel )
	{
		memory_app_use_to_total_memory_ratio = (float)(total_memory_in_use_by_application * 100 )/  \
		(float)(cumulative_vm_pages_claimed_from_kernel * SYSTEM_PAGE_SIZE );
	}

	printf(ANSI_COLOR_MAGENTA "memory in use by application = %f%% \n" ANSI_COLOR_RESET ,
				memory_app_use_to_total_memory_ratio );
	
	printf("total memory being used by memory manager = %lu bytes \n" ,
			((cumulative_vm_pages_claimed_from_kernel * SYSTEM_PAGE_SIZE)  \
			+ (number_of_struct_families * sizeof(vm_page_family_t))));

}


void mm_print_block_usage()
{
	vm_page_t* vm_page_curr ;
	vm_page_family_t* vm_page_family_curr ;
	block_meta_data_t* block_meta_data_curr ;
	uint32_t total_block_count , free_block_count , occupied_block_count ;
	uint32_t application_memory_usage ;

	ITERATE_PAGE_FAMILIES_BEGIN(first_vm_page_for_families , vm_page_family_curr )
	{
		total_block_count = 0 ;
		free_block_count = 0 ;
		application_memory_usage = 0 ;
		occupied_block_count = 0 ;
		ITERATE_VM_PAGE_PER_FAMILY_BEGIN( vm_page_family_curr , vm_page_curr )
		{

			ITERATE_VM_PAGE_ALL_BLOCKS_BEGIN( vm_page_curr , block_meta_data_curr )
			{
				total_block_count++ ;

				// sanity checks
				if(block_meta_data_curr->is_free == MM_FALSE )
				{
					assert(IS_GLTHREAD_LIST_EMPTY(&block_meta_data_curr->priority_thread_glue));
				}

				if(block_meta_data_curr->is_free == MM_TRUE )
				{
					assert(!IS_GLTHREAD_LIST_EMPTY(&block_meta_data_curr->priority_thread_glue));
				}

				if(block_meta_data_curr->is_free = MM_TRUE )
				{
					free_block_count++ ;
				}

				else
				{
					application_memory_usage += block_meta_data_curr->block_size + sizeof(block_meta_data_t) ;
					occupied_block_count++ ;
				}
			} ITERATE_VM_PAGE_ALL_BLOCKS_END( vm_page_curr , block_meta_data_curr );
		} ITERATE_VM_PAGE_PER_FAMILY_END( vm_page_family_curr , vm_page_curr );

		printf("%-20s	TBC : %-4u	FBC : %-4u	OBC : %-4u	app mem usage : %u \n" , 
				vm_page_family_curr->struct_name , total_block_count , free_block_count , occupied_block_count ,
				application_memory_usage );

	} ITERATE_PAGE_FAMILIES_END(first_vm_page_for_families ,  vm_page_family_curr );
}


static int mm_get_hard_internal_memory_frag_size
( block_meta_data_t* first , block_meta_data_t* second )
{
	block_meta_data_t* next_block = NEXT_META_BLOCK_BY_SIZE(first);
	return (int) ( (unsigned long)second - (unsigned long)first );
}


void xfree( void* app_data )
{
	block_meta_data_t *block_meta_data = ( block_meta_data_t* )
	((char*)app_data - sizeof(block_meta_data_t)) ;

	assert(block_meta_data->is_free == MM_FALSE );
	mm_free_blocks(block_meta_data);
}

/*
int main(int argv , char** argc){

	mm_init();
	printf("vm_page size : %lu\n",SYSTEM_PAGE_SIZE);
	void *addr1 = mm_get_new_vm_page_from_kernel(1);
	void *addr2 = mm_get_new_vm_page_from_kernel(1);// calling kernel to give a single page
	printf("The address of addr1 is %p and addr2 is %p .\n",addr1,addr2);
	printf("The address are not continuous as differnce between them is : %lu\n",addr2-addr1);
	return 0;	
}
*/




