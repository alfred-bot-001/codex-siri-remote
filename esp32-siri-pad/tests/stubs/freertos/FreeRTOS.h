#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>
#include <cstring>
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
struct TestQueue{size_t capacity,size;std::deque<std::vector<uint8_t>> items;};
using QueueHandle_t=TestQueue*;
inline QueueHandle_t xQueueCreate(size_t n,size_t s){return new TestQueue{n,s,{}};}
inline int xQueueSend(QueueHandle_t q,const void *p,unsigned){if(q->items.size()==q->capacity)return 0;auto b=(const uint8_t*)p;q->items.emplace_back(b,b+q->size);return 1;}
inline int xQueueReceive(QueueHandle_t q,void *p,unsigned){if(q->items.empty())return 0;memcpy(p,q->items.front().data(),q->size);q->items.pop_front();return 1;}
inline void xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,unsigned,void*,unsigned){}
