/**
 * @file none.c
 * @brief: Null gauge kernel to measure profiling overhead (Observer Effect)
 */
 #include <stdint.h>
 #include "../pterr.h"
 
 int init_gauge_none(void) {
     return PTERR_SUCCESS;
 }
 
 void run_gauge_none(int64_t n) {
     // 1. 消耗掉参数 n，防止编译器报 "unused parameter" 警告
     (void)n;
 
     // 2. 核心：插入一个空的 volatile 汇编块
     // 作用：这不会生成任何真实的机器码（0 cycle），但会强制编译器将此处视为一个不可跨越的屏障。
     // 它能严防编译器在 -O2/-O3 或 LTO（链接期优化）时把整个 run_gauge_none() 函数调用直接优化“消失”。
     __asm__ __volatile__("");
 }
 
 void cleanup_gauge_none(void) {
     return;
 }