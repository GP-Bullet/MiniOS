#ifndef LIB_USER_ASSERT
#define LIB_USER_ASSERT

#define NULL ((void*)0)

void user_spin(char* filename, int line, const char* func,
               const char* condition);
#define panic(...) user_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef NDEBUG
#define assert(CONDITION) ((void)0)
#else
#define assert(CONDITION) \
  if (!(CONDITION)) {     \
    panic(#CONDITION);    \
  }

#endif
#endif /* LIB_USER_ASSERT */
