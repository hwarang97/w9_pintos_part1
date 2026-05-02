#ifndef THREADS_INTR_STUBS_H
#define THREADS_INTR_STUBS_H

/* interrupt stub.
 *
 * 이것들은 intr-stubs.S 안의 작은 코드 조각들로, 가능한 256개 x86 interrupt마다
 * 하나씩 존재한다. 각각은 약간의 stack 조작을 한 뒤 intr_entry()로 jump한다.
 * 자세한 내용은 intr-stubs.S를 참고한다.
 *
 * 이 배열은 intr_init()이 쉽게 찾을 수 있도록 각 interrupt stub entry point를
 * 가리킨다. */
typedef void intr_stub_func (void);
extern intr_stub_func *intr_stubs[256];

#endif /* threads/intr-stubs.h */
