#ifndef PS4_PSC_RUNTIME_PLAN_H
#define PS4_PSC_RUNTIME_PLAN_H
#include <stdint.h>
typedef struct Ps4PscTransition {
 uint16_t tx,ty,kind,map_id,x,y;
 uint8_t facing,align;
} Ps4PscTransition;
typedef struct Ps4PscRuntimePlan {
 uint16_t manager_offset,update_offset,event_offset,trans1_offset,trans2_offset;
 uint8_t manager_count,update_count,event_count,trans1_count,trans2_count,valid;
} Ps4PscRuntimePlan;
extern const uint8_t ps4_psc_manager_ids[];
extern const uint8_t ps4_psc_update_ids[];
extern const uint8_t ps4_psc_event_ids[];
extern const Ps4PscTransition ps4_psc_transitions[];
const Ps4PscRuntimePlan* ps4_psc_runtime_plan_get(uint16_t map_id);
#endif
