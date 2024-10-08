#include<stdio.h>
#include<memory.h>
#include<unistd.h>  // for geting page size
#include<sys/mman.h> // for using mmap()
#include<stdint.h>
#include"mm.h"
#include<assert.h>

static vm_page_for_families_t* first_vm_page_for_families = NULL ;
static size_t SYSTEM_PAGE_SIZE = 0 ;



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
	} ITERATE_PAGE_FAMILIES_END(vm_page_for_families_ptr,vm_page_family_curr );
	if (count == MAX_FAMILIES_PER_VM_PAGE)
	{
		new_vm_page_for_families = (vm_page_for_families_t*) mm_get_new_vm_page_from_kernel(1) ;
		new_vm_page_for_families->next = first_vm_page_for_families ;
		first_vm_page_for_families = new_vm_page_for_families ;
		vm_page_family_curr = &first_vm_page_for_families->vm_page_family[0];
	}

	strncpy(vm_page_family_curr->struct_name , struct_name , MM_MAX_STRUCT_NAME );
	vm_page_family_curr->struct_size = struct_size ;
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




