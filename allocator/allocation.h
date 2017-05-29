#include <iostream>
#include "stdlib.h"
#include "stdio.h"
enum {__ALIGN = 8};     // 边界对齐
enum {__MAX_BYTES = 128};   // 最大内存块
enum {__NFREELISTS = __MAX_BYTES / __ALIGN};    // free list 个数


template <bool threads, int inst>
class __default_alloc_template {
private:
    //对齐为8的整数
    static size_t ROUND_UP(size_t bytes) {
        return ((bytes + __ALIGN - 1) & (~(__ALIGN - 1)));
    }

private:
    union obj {
        union obj * free_list_link;
        char client_data[1];
    };

private:
    static obj *volatile free_list[__NFREELISTS];
    static size_t FREELIST_INDEX(size_t bytes) {
        return ((bytes + __ALIGN - 1) / __ALIGN - 1);
    }
    // 返回一个大小为n的内存，并加入到指向n大小的内存的free_list
    static void *refill(size_t n);

    // 从内存池中区出大小为size * nobjs内存，如果内存池不够则重新分配内存
    static char *chunk_alloc(size_t size, int &nobjs);

    static char* start_free;    // 内存池其实位置
    static char* end_free;      // 内存池结束位置
    static size_t heap_size;

public:
    static void * allocate(size_t n);
    static void deallocate(void *p, size_t n);
    static void *reallocate(void *p, size_t old_sz, size_t new_sz);

};

////////////////////////////////////////////////////////////

template <bool threads, int ints>
char *__default_alloc_template<threads, ints>::start_free = 0;

template <bool threads, int ints>
char* __default_alloc_template<threads, ints>::end_free = 0;

template <bool threads, int ints>
size_t __default_alloc_template<threads, ints>::heap_size = 0;


template <bool threads, int ints>
void * __default_alloc_template<threads, ints>::allocate(size_t n) {
    obj* volatile * my_free_list;
    fprintf(stdout, "用户请求内存%d\n", n);
    if (n > __MAX_BYTES) {
        return malloc(n);
    }    
    my_free_list = free_list + FREELIST_INDEX(n);
    if (*my_free_list == NULL) {
        void * p = refill(ROUND_UP(n));
        return p;
    }
    obj * rc = *my_free_list;
    *my_free_list = rc->free_list_link;
    return rc;
}

template <bool threads, int ints>
void __default_alloc_template<threads, ints>::deallocate(void *p, size_t n) {
    obj* e =  (obj*)p;
    obj* volatile * my_free_list;
    fprintf(stdout, "收回内存%d\n", n);
    if (n > __MAX_BYTES) {
        free(n);
    }
    my_free_list = free_list + FREELIST_INDEX(n);
    obj * tmp = *my_free_list;
    *my_free_list = e;
    e->free_list_link = tmp;
}

template <bool threads, int ints>
void* __default_alloc_template<threads, ints>::reallocate(void *p, size_t old_sz, size_t new_sz) {

    return NULL;
}

//从内存池中取出空间给free_list
template <bool threads, int ints>
char* __default_alloc_template<threads, ints>::chunk_alloc(size_t size, int& nobjs) {
    int bytes_left = end_free - start_free; // 内存池剩余空间大小
    int total_bytes = size * nobjs;
    if (total_bytes < bytes_left) { 
        // 如果内存池空间还剩余很多
        char *rc = start_free;
        start_free += total_bytes;
        fprintf(stdout, "从内存池中提取%d字节，剩余%d字节\n", total_bytes, bytes_left - total_bytes);
        return rc;
    } else if (bytes_left >= size) { 
        // 如果内存池空间小于 size * nobjs，但还能提供一个以上的内存块
        nobjs = bytes_left / size;
        char *rc = start_free;
        start_free += size * nobjs; 
        fprintf(stdout, "从内存池中提取%d字节，剩余%d字节\n", size * nobjs, bytes_left - size*nobjs);
        return rc;
    } else {
        // 如果内存池中没有内存
        size_t bytes_to_get = 2 * total_bytes + ROUND_UP(heap_size >> 4);
        fprintf(stdout, "创建内存池内存 %d字节\n", bytes_to_get);
        if (bytes_left > 0) {
            // 如果还有残余的空间，将其分配给相应的free_list
            obj *volatile * my_free_list = free_list + FREELIST_INDEX(bytes_left);
            ((obj*)start_free)->free_list_link = *my_free_list;
            *my_free_list = (obj*)start_free;
            start_free += bytes_left;
        }
        // 开始重新分配内存
        start_free = (char*)malloc(bytes_to_get);
        if (NULL == start_free) {
           //　malloc失败，free_list中寻找块区大，且空闲的块。
           for (int i = size; i <= __MAX_BYTES; i += __ALIGN) {
               obj* volatile * my_free_list = free_list + FREELIST_INDEX(i);
               obj* p = *my_free_list;
               if (0 != p) {
                   *my_free_list = p->free_list_link;
                   start_free = (char*)p;
                   end_free = start_free + i;
                   return chunk_alloc(size, nobjs);
               }
           }
        }
        heap_size += bytes_to_get;
        end_free = start_free + bytes_to_get;
        return (chunk_alloc(size, nobjs));
    }
    return NULL;
}

template <bool threads, int ints>
void * __default_alloc_template<threads, ints>::refill(size_t n) {
    int obj_num = 20;
    //从内存池中取出obj_num * n大小的内存块
    char *chunk = chunk_alloc(n, obj_num);
    obj* volatile *my_free_list;
    if (1 == obj_num)
        return chunk;

    my_free_list = free_list + FREELIST_INDEX(n);
    obj* current_obj = (obj*)chunk;
    obj* rc = current_obj; // 返回给用户
    obj* next_obj = (obj*)(chunk + n);
    // 将obj_num个内存块通过free_list串起来
    for (int i = 1; i < obj_num -1; i++) {
        current_obj = next_obj;
        next_obj = (obj*)((char*)current_obj + n);
        current_obj->free_list_link = next_obj;
    }
    next_obj->free_list_link = 0;
    return rc;
}

template <bool threads, int ints>
typename __default_alloc_template<threads, ints>::obj * volatile  __default_alloc_template<threads, ints>::free_list[__NFREELISTS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

typedef __default_alloc_template<false, 0> alloc;
template<class T, class Alloc>
class simple_alloc {
public:
    static T*allocate(size_t n) {
        return 0 == n? 0: (T*)Alloc::allocate(n * sizeof(T));

    }
    static T *allocate(void) {
        return Alloc::allocate(sizeof(T));
    }
    static void deallocate(T*p, size_t n) {
        if (0 != n)
            Alloc::deallocate(p, n*sizeof(T));
    }
    static void deallocate(T *p) {
        Alloc::deallocate(p, sizeof(T));
    }

};


