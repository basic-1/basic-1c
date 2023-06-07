8x8 unsigned multiplication - use `MUL`  
16x16 signed and unsigned multiplication - use `__LIB_COM_MUL16` function  
8x8 unsigned division - use `DIV`  
16x16 unsigned division - use `DIVW`  
16x16 signed division - use `__LIB_COM_DIV16` function  
8x8 unsigned remainder - use `DIV`  
16x16 unsigned remainder - use `DIVW`  
16x16 signed remainder - use `__LIB_COM_REM16` function  
  
Run-time errors:  
1 `B1C_RTE_ARR_ALLOC` - array is already allocated  
2 `B1C_RTE_STR_TOOLONG` - string is too long  
3 `B1C_RTE_MEM_NOT_ENOUGH` - not enough memory  
4 `B1C_RTE_STR_WRONG_INDEX` - wrong character index  
5 `B1C_RTE_STR_INV_NUM` - invalid string representation of an integer  
6 `B1C_RTE_ARR_UNALLOC` - unallocated array access  
7 `B1C_RTE_COM_WRONG_INDEX` - common wrong index error  
8 `B1C_RTE_STR_INVALID` - invalid quoted string  
  
