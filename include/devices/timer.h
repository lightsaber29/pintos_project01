#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>

#include "threads/synch.h"  // lock과 condition 구조체를 사용하기 위해 추가
#include "threads/thread.h" // thread 구조체 사용
#include "list.h"           // list_elem을 사용하기 위해 추가

/* Number of timer interrupts per second. */
#define TIMER_FREQ 100

struct alarm {
    int64_t wake_up_time;   // 스레드가 깨어날 시간 (절대 시간)
    struct condition cond;  // 조건 변수
    struct lock lock;       // 동기화를 위한 뮤텍스(동기화 데이터)
    struct thread *thread;   // 알람을 설정한 스레드
    struct list_elem elem;   // 리스트에 연결하기 위한 요소. list.c 에 이렇게 씀
};

void timer_init (void);
void timer_calibrate (void);

int64_t timer_ticks (void);
int64_t timer_elapsed (int64_t);

void timer_sleep (int64_t ticks);
void timer_msleep (int64_t milliseconds);
void timer_usleep (int64_t microseconds);
void timer_nsleep (int64_t nanoseconds);

void timer_print_stats (void);

void alarm_init(struct alarm *a);
void alarm_set(struct alarm *a, int64_t ticks);
bool wake_time_less(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);


#endif /* devices/timer.h */
