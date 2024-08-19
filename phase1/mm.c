#include<stdio.h>
#include<memory.h>
#include<unistd.h>  // for geting page size
#include<sys/mman.h> // for using mmap()

static size_t SYSTEM_PAGE_SIZE = 0 ;
static vm_page_for_families_t* first_vm_page_for_families = NULL ;

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
                        MAP_ANON|MAP_PRIVATE,
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

void mm_instantiate_new_page_family(char* struct_name , uint32_t struct_size)
{
        vm_page_family_t *vm_page_family_curr = NULL ;
        vm_page_for_families_t *new_vm_page_for_families = NULL ;
        if (struct_size > SYSTEM_PAGE_SIZE )
        {
                printf("Error : %s() Structutre %s Size exceeds system page size\n",__FUNCTION__,struct_name);
                return ;
        }
        if(!first_vm_page_for_families)
        {
                first_vm_page_for_families = (vm_page_for_families_t*)mm_get_new_vm_page_from_kernel(1);
                first_vm_page_for_families->next = NULL ;
                strncpy(first_vm_page_for_families->vm_page_family[0].struct_name , struct_name , MM_MAX_STRUCT_NAME);
                first_vm_page_for_families->vm_page_family[0].struct_size = struct_size ;
                return ;
        }
}

int main(int argv , char** argc){

        mm_init();
        printf("vm_page size : %lu\n",SYSTEM_PAGE_SIZE);
        void *addr1 = mm_get_new_vm_page_from_kernel(1);
        void *addr2 = mm_get_new_vm_page_from_kernel(1);// calling kernel to give a single page
        printf("The address of addr1 is %p and addr2 is %p .\n",addr1,addr2);
        return 0;
}