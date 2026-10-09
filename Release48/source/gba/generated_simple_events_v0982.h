/* Auto-generated from verified PSIV US ROM by generate_simple_event_descriptors.py. */
#ifndef PS4_GENERATED_SIMPLE_EVENTS_V0982_H
#define PS4_GENERATED_SIMPLE_EVENTS_V0982_H
struct Ps4NoopEventDescriptor { uint8_t event_index; };
static const struct Ps4NoopEventDescriptor ps4_noop_event_descriptors[4]={
    {0x00u}, /* $06B2D2, 2 bytes */
    {0x01u}, /* $06B2D4, 2 bytes */
    {0x02u}, /* $06B2D6, 2 bytes */
    {0x05u}, /* $06B4B0, 2 bytes */
};
#define PS4_NOOP_EVENT_DESCRIPTOR_COUNT 4u
struct Ps4SimpleEventDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; };
static const struct Ps4SimpleEventDescriptor ps4_simple_event_descriptors[16]={
    {0x0Fu,0x6Eu,0x0Eu}, /* $06BEE8, 16 bytes */
    {0x28u,0x27u,0x32u}, /* $06DBD8, 16 bytes */
    {0x2Au,0x0Cu,0x73u}, /* $06DEAE, 16 bytes */
    {0x3Du,0x04u,0x86u}, /* $06FAD4, 18 bytes */
    {0x3Eu,0x05u,0x87u}, /* $06FAE6, 18 bytes */
    {0x55u,0x31u,0xA4u}, /* $070ADA, 18 bytes */
    {0x57u,0x34u,0xA5u}, /* $070B0C, 18 bytes */
    {0x58u,0x35u,0x9Fu}, /* $070B1E, 18 bytes */
    {0x7Cu,0x24u,0x2Eu}, /* $072B1C, 16 bytes */
    {0x8Du,0x00u,0xB4u}, /* $07318E, 18 bytes */
    {0x8Eu,0x03u,0xB5u}, /* $0731A0, 18 bytes */
    {0x92u,0x07u,0xC7u}, /* $0733BA, 18 bytes */
    {0x9Au,0x0Eu,0xC2u}, /* $073880, 18 bytes */
    {0x9Bu,0x0Fu,0xC3u}, /* $073892, 18 bytes */
    {0x9Cu,0x10u,0xC4u}, /* $0738A4, 18 bytes */
    {0xA0u,0x6Du,0x15u}, /* $073ECE, 18 bytes */
};
#define PS4_SIMPLE_EVENT_DESCRIPTOR_COUNT 16u
struct Ps4FacingDialogueFlagDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t dialogue; uint8_t flag; };
static const struct Ps4FacingDialogueFlagDescriptor ps4_facing_dialogue_flag_descriptors[16]={
    {0x51u,0u,0x35u,0xA2u}, /* source obj $C300, event $070A4E */
    {0x52u,0u,0x36u,0xA3u}, /* source obj $C300, event $070A76 */
    {0x72u,0u,0x21u,0x1Cu}, /* source obj $C300, event $072678 */
    {0x73u,7u,0x2Cu,0x20u}, /* source obj $C4C0, event $07269E */
    {0x75u,1u,0x5Eu,0x1Fu}, /* source obj $C340, event $072872 */
    {0x76u,0u,0x6Cu,0x27u}, /* source obj $C300, event $072898 */
    {0x7Au,0u,0x2Du,0x2Du}, /* source obj $C300, event $072AD0 */
    {0x7Bu,0u,0x2Eu,0x3Cu}, /* source obj $C300, event $072AF6 */
    {0x7Fu,1u,0x13u,0x79u}, /* source obj $C340, event $072C08 */
    {0x80u,1u,0x5Fu,0x7Au}, /* source obj $C340, event $072C2E */
    {0x81u,0u,0x50u,0x7Bu}, /* source obj $C300, event $072C54 */
    {0x84u,0u,0x39u,0xA9u}, /* source obj $C300, event $072D44 */
    {0x85u,0u,0x3Au,0xABu}, /* source obj $C300, event $072D6C */
    {0x87u,0u,0x3Cu,0xAEu}, /* source obj $C300, event $072FB4 */
    {0x8Bu,2u,0x6Cu,0xB3u}, /* source obj $C380, event $07313E */
    {0x8Cu,2u,0x6Du,0xB7u}, /* source obj $C380, event $073166 */
};
#define PS4_FACING_DIALOGUE_FLAG_DESCRIPTOR_COUNT 16u
struct Ps4SimpleBattleEventDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; uint8_t battle_index; uint8_t set_mapload_bit; uint8_t return_one; };
static const struct Ps4SimpleBattleEventDescriptor ps4_simple_battle_event_descriptors[6]={
    {0x53u,0xFFu,0x8Bu,0x0Du,1u,0u}, /* $070A9E, 30 bytes */
    {0x54u,0xFFu,0x92u,0x0Bu,1u,0u}, /* $070ABC, 30 bytes */
    {0x56u,0x33u,0x93u,0x0Cu,0u,0u}, /* $070AEC, 32 bytes */
    {0x60u,0x03u,0xD4u,0x16u,0u,0u}, /* $07144A, 32 bytes */
    {0x61u,0x01u,0xD2u,0x17u,0u,0u}, /* $07146A, 32 bytes */
    {0x8Au,0xFFu,0xB2u,0x14u,1u,1u}, /* $07311E, 32 bytes */
};
#define PS4_SIMPLE_BATTLE_EVENT_DESCRIPTOR_COUNT 6u
struct Ps4TempFlagClearDescriptor { uint8_t event_index; uint8_t flag; };
static const struct Ps4TempFlagClearDescriptor ps4_temp_flag_clear_descriptors[2]={
    {0x3Cu,0x18u}, /* $06FACC, 8 bytes */
    {0x95u,0x1Bu}, /* $0735EE, 8 bytes */
};
#define PS4_TEMP_FLAG_CLEAR_DESCRIPTOR_COUNT 2u
struct Ps4DialogueTwoFlagsDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t first_flag; uint8_t second_flag; };
static const struct Ps4DialogueTwoFlagsDescriptor ps4_dialogue_two_flags_descriptors[1]={
    {0x9Du,0x11u,0xC9u,0xC8u}, /* $0738B6, 28 bytes */
};
#define PS4_DIALOGUE_TWO_FLAGS_DESCRIPTOR_COUNT 1u
struct Ps4DialogueRecoveryDescriptor { uint8_t event_index; uint8_t dialogue; };
static const struct Ps4DialogueRecoveryDescriptor ps4_dialogue_recovery_descriptors[1]={
    {0x67u,0x2Bu}, /* $071E56, 16 bytes */
};
#define PS4_DIALOGUE_RECOVERY_DESCRIPTOR_COUNT 1u
struct Ps4DialogueCameraFlagDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; };
static const struct Ps4DialogueCameraFlagDescriptor ps4_dialogue_camera_flag_descriptors[1]={
    {0x50u,0x02u,0xC6u}, /* $070A2A, 36 bytes */
};
#define PS4_DIALOGUE_CAMERA_FLAG_DESCRIPTOR_COUNT 1u
struct Ps4ConditionalDialogueFlagDescriptor { uint8_t event_index; uint8_t flag; uint8_t first_dialogue; uint8_t repeat_dialogue; };
static const struct Ps4ConditionalDialogueFlagDescriptor ps4_conditional_dialogue_flag_descriptors[1]={
    {0x90u,0xB9u,0x01u,0x02u}, /* $0731DA, 38 bytes */
};
#define PS4_CONDITIONAL_DIALOGUE_FLAG_DESCRIPTOR_COUNT 1u
struct Ps4TreeDialogueFlagDescriptor { uint8_t event_index; uint32_t dialogue_tree; uint8_t dialogue; uint8_t flag; };
static const struct Ps4TreeDialogueFlagDescriptor ps4_tree_dialogue_flag_descriptors[1]={
    {0x43u,0x001E99F0u,0x06u,0x80u}, /* $06FC76, 30 bytes */
};
#define PS4_TREE_DIALOGUE_FLAG_DESCRIPTOR_COUNT 1u
struct Ps4ObjectDialoguePatchDescriptor { uint8_t event_index; uint32_t pre_tree; uint8_t dialogue; uint8_t start_slot; uint8_t count; uint8_t object_dialogue; uint32_t post_tree; uint8_t flag; };
static const struct Ps4ObjectDialoguePatchDescriptor ps4_object_dialogue_patch_descriptors[2]={
    {0x45u,0x001EDCA0u,0x2Eu,3u,2u,0x23u,0x001EE9B0u,0x96u}, /* $06FE1C, 58 bytes */
    {0x49u,0xFFFFFFFFu,0x1Cu,0u,2u,0x1Du,0xFFFFFFFFu,0x90u}, /* $07069E, 34 bytes */
};
#define PS4_OBJECT_DIALOGUE_PATCH_DESCRIPTOR_COUNT 2u
struct Ps4QuickDialogueBattleEventDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; uint8_t battle_index; uint8_t mapload_bit; uint8_t return_one; uint8_t saved_sound; };
static const struct Ps4QuickDialogueBattleEventDescriptor ps4_quick_dialogue_battle_event_descriptors[5]={
    {0x3Fu,0x06u,0x83u,0x09u,0x07u,0u,0xFFu}, /* $06FAF8, 38 bytes */
    {0x4Eu,0x3Au,0x9Eu,0x11u,0x07u,0u,0xFEu}, /* $070976, 44 bytes */
    {0x59u,0x36u,0x9Au,0x0Eu,0x03u,0u,0xFFu}, /* $070B30, 38 bytes */
    {0x7Du,0x2Fu,0x2Fu,0x07u,0x03u,1u,0xFFu}, /* $072B2C, 38 bytes */
    {0x8Fu,0x04u,0xB6u,0x15u,0x03u,1u,0xFFu}, /* $0731B2, 40 bytes */
};
#define PS4_QUICK_DIALOGUE_BATTLE_EVENT_DESCRIPTOR_COUNT 5u
struct Ps4GuardedQuickBattleDescriptor { uint8_t event_index; uint8_t guard_flag; uint8_t fallback_dialogue; uint8_t quick_dialogue; uint8_t flag; uint8_t battle_index; uint8_t mapload_bit; };
static const struct Ps4GuardedQuickBattleDescriptor ps4_guarded_quick_battle_descriptors[1]={
    {0x88u,0xAEu,0x35u,0x36u,0xAFu,0x13u,0x03u}, /* $072FDC, 60 bytes */
};
#define PS4_GUARDED_QUICK_BATTLE_DESCRIPTOR_COUNT 1u
struct Ps4ExtendedBattleEventDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; uint8_t battle_index; uint8_t set_mapload_bit; uint8_t return_one; uint8_t guard_flag; uint8_t saved_sound; };
static const struct Ps4ExtendedBattleEventDescriptor ps4_extended_battle_event_descriptors[2]={
    {0x6Bu,0xFFu,0x0Bu,0x00u,0u,1u,0x0Bu,0xFFu}, /* $0722D2, 36 bytes */
    {0x71u,0xFFu,0x1Bu,0x02u,1u,1u,0xFFu,0x9Eu}, /* $072654, 36 bytes */
};
#define PS4_EXTENDED_BATTLE_EVENT_DESCRIPTOR_COUNT 2u
struct Ps4PartyFormationBattleDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; uint8_t item; uint8_t battle_index; uint8_t mapload_bit; uint8_t source_facing; uint16_t x0,y0,x1,y1,x2,y2,x3,y3,x4,y4; };
static const struct Ps4PartyFormationBattleDescriptor ps4_party_formation_battle_descriptors[1]={
    {0x5Au,0x3Cu,0xA6u,0x8Eu,0x0Fu,3u,0x00u,0x01E0u,0x0190u,0x01F0u,0x0190u,0x01D0u,0x0190u,0x01F0u,0x01A0u,0x01D0u,0x01A0u}, /* $070B56, 218 bytes */
};
#define PS4_PARTY_FORMATION_BATTLE_DESCRIPTOR_COUNT 1u
struct Ps4InventoryRewardDescriptor { uint8_t event_index; uint8_t item; uint8_t success_dialogue; uint8_t full_dialogue; uint8_t flag; };
static const struct Ps4InventoryRewardDescriptor ps4_inventory_reward_descriptors[1]={
    {0x4Au,0x53u,0x2Du,0x2Eu,0x91u}, /* $0706C0, 66 bytes */
};
#define PS4_INVENTORY_REWARD_DESCRIPTOR_COUNT 1u
struct Ps4MoneyDialogueTransactionDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t dialogue; uint8_t money_flag; uint8_t success_flag; uint8_t patch_dialogue; uint8_t flag_when_affordable; uint32_t cost; };
static const struct Ps4MoneyDialogueTransactionDescriptor ps4_money_dialogue_transaction_descriptors[2]={
    {0x4Bu,16u,0x12u,0x8Cu,0x8Au,0x15u,0u,0x000001F4u}, /* source obj $C700, event $070702, 114 bytes */
    {0x82u,0u,0x51u,0x7Fu,0x7Cu,0xFFu,1u,0x0000C350u}, /* source obj $C300, event $072C7A, 98 bytes */
};
#define PS4_MONEY_DIALOGUE_TRANSACTION_DESCRIPTOR_COUNT 2u
struct Ps4CoordObjectConditionalDialogueDescriptor { uint8_t event_index; uint8_t flag; uint8_t clear_dialogue; uint8_t set_dialogue; uint16_t x0,y0; uint8_t slot0; uint16_t x1,y1; uint8_t slot1; uint16_t x2,y2; uint8_t slot2; uint16_t x3,y3; uint8_t slot3; };
static const struct Ps4CoordObjectConditionalDialogueDescriptor ps4_coord_object_conditional_dialogue_descriptors[1]={
    {0x93u,0x9Eu,0x3Cu,0x3Du,0x0110u,0x0260u,19u,0x0110u,0x01A0u,15u,0x0200u,0x0140u,13u,0x0230u,0x0140u,17u}, /* table $073420, event $0733CC, 116 bytes */
};
#define PS4_COORD_OBJECT_CONDITIONAL_DIALOGUE_DESCRIPTOR_COUNT 1u
struct Ps4PairedObjectDialogueStateDescriptor { uint8_t event_index; uint8_t slot0,slot1; uint8_t primary_flag,secondary_flag; uint8_t base_dialogue,primary_dialogue,secondary_dialogue; uint8_t temp_flag; };
static const struct Ps4PairedObjectDialogueStateDescriptor ps4_paired_object_dialogue_state_descriptors[1]={
    {0x4Fu,0u,1u,0x9Eu,0x95u,0x00u,0x02u,0x01u,0x1Au}, /* objs $C300/$C340, event $0709A2, 136 bytes */
};
#define PS4_PAIRED_OBJECT_DIALOGUE_STATE_DESCRIPTOR_COUNT 1u
struct Ps4RngDialogueDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t flag; uint8_t zero_dialogue,one_dialogue,other_dialogue,flagged_dialogue; };
static const struct Ps4RngDialogueDescriptor ps4_rng_dialogue_descriptors[1]={
    {0x5Bu,4u,0x9Eu,0x5Eu,0x5Fu,0x5Du,0x60u}, /* obj $C400, event $070C30, 82 bytes */
};
#define PS4_RNG_DIALOGUE_DESCRIPTOR_COUNT 1u
struct Ps4DialogueSuccessorFlagDescriptor { uint8_t event_index; uint8_t flag; uint8_t first_dialogue; uint8_t repeat_dialogue; uint8_t successor_marker; };
static const struct Ps4DialogueSuccessorFlagDescriptor ps4_dialogue_successor_flag_descriptors[1]={
    {0x5Cu,0x81u,0x31u,0x32u,0x35u}, /* $070C82, 50 bytes */
};
#define PS4_DIALOGUE_SUCCESSOR_FLAG_DESCRIPTOR_COUNT 1u
struct Ps4DialogueFaceResumeDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t dialogue; };
static const struct Ps4DialogueFaceResumeDescriptor ps4_dialogue_face_resume_descriptors[1]={
    {0x22u,0u,0x3Du}, /* source obj $C300, event $06D468, 62 bytes */
};
#define PS4_DIALOGUE_FACE_RESUME_DESCRIPTOR_COUNT 1u
struct Ps4ChoreographedFollowupDialogueDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t source_facing; uint8_t dialogue; uint16_t state_addr; uint8_t state_value; uint8_t flag; };
static const struct Ps4ChoreographedFollowupDialogueDescriptor ps4_choreographed_followup_dialogue_descriptors[3]={
    {0x6Du,1u,0x04u,0x0Au,0xF8E6u,0x20u,0x5Eu}, /* source obj $C340, event $072428, 124 bytes */
    {0x6Eu,7u,0x04u,0x08u,0xF8E7u,0x0Eu,0x74u}, /* source obj $C4C0, event $0724A4, 124 bytes */
    {0x6Fu,8u,0x04u,0x06u,0xF8E8u,0x0Fu,0x90u}, /* source obj $C500, event $072520, 126 bytes */
};
#define PS4_CHOREOGRAPHED_FOLLOWUP_DIALOGUE_DESCRIPTOR_COUNT 3u
struct Ps4ConsumeItemDialogueDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t item; uint8_t success_dialogue; uint8_t flag; uint8_t fallback_dialogue; };
static const struct Ps4ConsumeItemDialogueDescriptor ps4_consume_item_dialogue_descriptors[1]={
    {0x78u,1u,0x93u,0x4Fu,0x29u,0x53u}, /* source obj $C340, event $0729E4, 66 bytes */
};
#define PS4_CONSUME_ITEM_DIALOGUE_DESCRIPTOR_COUNT 1u
struct Ps4LayoutAnimationDescriptor { uint8_t event_index; uint8_t sound; uint16_t x_chunk,y_chunk; uint8_t plane; uint32_t frames_addr; uint8_t flag; uint8_t frame_count; };
static const struct Ps4LayoutAnimationDescriptor ps4_layout_animation_descriptors[1]={
    {0x41u,0xE2u,0x000Cu,0x0009u,1u,0x0006FBC4u,0x48u,8u}, /* $06FB5E, 144 bytes */
};
#define PS4_LAYOUT_ANIMATION_DESCRIPTOR_COUNT 1u
struct Ps4DynamicTwoCellLayoutDescriptor { uint8_t event_index,sound,expected_lower; uint32_t frames_addr; uint8_t frame_count; };
static const struct Ps4DynamicTwoCellLayoutDescriptor ps4_dynamic_two_cell_layout_descriptors[2]={
    {0x35u,0xD9u,0x38u,0x0006F4D4u,9u}, /* $06F43A, 182 bytes */
    {0x36u,0xD9u,0x38u,0x0006F588u,9u}, /* $06F4F0, 180 bytes */
};
#define PS4_DYNAMIC_TWO_CELL_LAYOUT_DESCRIPTOR_COUNT 2u
struct Ps4FixedTwoRowLayoutDescriptor { uint8_t event_index,sound; uint16_t x_chunk,y_chunk; uint8_t plane; uint32_t frames_addr; uint8_t frame_count; };
static const struct Ps4FixedTwoRowLayoutDescriptor ps4_fixed_two_row_layout_descriptors[1]={
    {0x42u,0xE2u,0x000Fu,0x0008u,1u,0x0006FC5Cu,5u}, /* $06FBEE, 136 bytes */
};
#define PS4_FIXED_TWO_ROW_LAYOUT_DESCRIPTOR_COUNT 1u
struct Ps4GaruberkDoorEnteredDescriptor { uint8_t event_index; uint8_t open_upper; uint32_t close_frames_addr; uint8_t frame_count; };
static const struct Ps4GaruberkDoorEnteredDescriptor ps4_garuberk_door_entered_descriptors[2]={
    {0x37u,0x3Eu,0x0006F714u,5u}, /* $06F5A4, 384 bytes */
    {0x38u,0x34u,0x0006F894u,5u}, /* $06F724, 384 bytes */
};
#define PS4_GARUBERK_DOOR_ENTERED_DESCRIPTOR_COUNT 2u
struct Ps4PaletteAlarmDialogueDescriptor { uint8_t event_index,sound,delay_down,delay_restore,dialogue,temp_flag; };
static const struct Ps4PaletteAlarmDialogueDescriptor ps4_palette_alarm_dialogue_descriptors[1]={
    {0x12u,0xDBu,3u,3u,0x0Au,0x08u}, /* $06C27C, 64 bytes */
};
#define PS4_PALETTE_ALARM_DIALOGUE_DESCRIPTOR_COUNT 1u
struct Ps4PaletteBlackBattleDescriptor { uint8_t event_index,object_slot,delay,dialogue,flag,battle_index; };
static const struct Ps4PaletteBlackBattleDescriptor ps4_palette_black_battle_descriptors[1]={
    {0x40u,0u,2u,0x48u,0x41u,0x03u}, /* source obj $C300, event $06FB1E, 64 bytes */
};
#define PS4_PALETTE_BLACK_BATTLE_DESCRIPTOR_COUNT 1u
#define PS4_EVTTR_DIALOGUE 1u
#define PS4_EVTTR_FLAG_SET 2u
#define PS4_EVTTR_SOUND 3u
#define PS4_EVTTR_FADE_OUT 4u
#define PS4_EVTTR_RAM16 5u
#define PS4_EVTTR_MAPLOAD_CLEAR 6u
#define PS4_EVTTR_REFRESH_MAP 7u
#define PS4_EVTTR_FACE_OBJECT 8u
#define PS4_EVTTR_FADE_IN 9u
#define PS4_EVTTR_RAM8 10u
#define PS4_EVTTR_TREE_LOAD 11u
#define PS4_EVTTR_REMOVE_ALL_ITEM 12u
#define PS4_EVTTR_TYPED_FLAG_SET 13u
#define PS4_EVTTR_COPY32 14u
#define PS4_EVTTR_COPY8 15u
#define PS4_EVTTR_BRANCH_FLAG_CLEAR 16u
#define PS4_EVTTR_RETURN 17u
#define PS4_EVTTR_RAM32 18u
#define PS4_EVTTR_PARTY_SYNC 19u
#define PS4_EVTTR_MOVE_CAMERA 20u
#define PS4_EVTTR_WAIT_UPDATES 21u
#define PS4_EVTTR_BRANCH_FLAG_SET 22u
#define PS4_EVTTR_ENSURE_SAVED_SOUND 23u
#define PS4_EVTTR_VINT 24u
#define PS4_EVTTR_FACE_OBJECT_OPPOSITE 25u
struct Ps4TransitionEventOp { uint8_t opcode; uint8_t arg8; uint16_t addr; uint32_t value; };
static const struct Ps4TransitionEventOp ps4_transition_event_ops[153]={
    {14u,0x00u,0xED54u,0x0000F40Au},
    {15u,0x00u,0xED58u,0x0000F40Eu},
    {18u,0x00u,0xF40Au,0x00FFFFFFu},
    {10u,0xFFu,0xF40Eu,0x00000000u},
    {19u,0x00u,0x0000u,0x00000000u},
    {3u,0xDFu,0x0000u,0x00000000u},
    {4u,0x00u,0x0000u,0x00000000u},
    {5u,0x00u,0xEC28u,0x000000FEu},
    {5u,0x00u,0xEC2Au,0x000000FDu},
    {5u,0x00u,0xEC48u,0x00000038u},
    {5u,0x00u,0xEC4Au,0x0000004Cu},
    {5u,0x00u,0xEC44u,0x00000004u},
    {5u,0x00u,0xEC46u,0x00000000u},
    {6u,0x03u,0xEC4Eu,0x00000000u},
    {7u,0x00u,0x0000u,0x00000000u},
    {20u,0x08u,0x01F0u,0x000001D0u},
    {9u,0x00u,0x0000u,0x00000000u},
    {3u,0xABu,0x0000u,0x00000000u},
    {21u,0x3Bu,0x0000u,0x00000000u},
    {20u,0x02u,0x01C0u,0x00000260u},
    {14u,0x00u,0xF40Au,0x0000ED54u},
    {15u,0x00u,0xF40Eu,0x0000ED58u},
    {3u,0xDFu,0x0000u,0x00000000u},
    {4u,0x00u,0x0000u,0x00000000u},
    {5u,0x00u,0xEC28u,0x000000FDu},
    {5u,0x00u,0xEC2Au,0x000000FEu},
    {5u,0x00u,0xEC48u,0x00000038u},
    {5u,0x00u,0xEC4Au,0x00000048u},
    {5u,0x00u,0xEC44u,0x00000004u},
    {5u,0x00u,0xEC46u,0x00000000u},
    {6u,0x03u,0xEC4Eu,0x00000000u},
    {7u,0x00u,0x0000u,0x00000000u},
    {9u,0x00u,0x0000u,0x00000000u},
    {16u,0xE1u,0x0000u,0x00000002u},
    {2u,0xE7u,0x0000u,0x00000000u},
    {17u,0x00u,0x0000u,0x00000000u},
    {3u,0x9Au,0x0000u,0x00000000u},
    {25u,0x04u,0x0000u,0x00000000u},
    {22u,0x27u,0x0000u,0x00000002u},
    {1u,0x52u,0x0000u,0x00000000u},
    {17u,0x00u,0x0000u,0x00000000u},
    {1u,0x4Cu,0x0000u,0x00000000u},
    {8u,0x04u,0x0000u,0x00000000u},
    {21u,0x09u,0x0000u,0x00000000u},
    {8u,0x04u,0x0000u,0x00000008u},
    {21u,0x09u,0x0000u,0x00000000u},
    {8u,0x04u,0x0000u,0x00000004u},
    {21u,0x09u,0x0000u,0x00000000u},
    {8u,0x04u,0x0000u,0x0000000Cu},
    {21u,0x09u,0x0000u,0x00000000u},
    {8u,0x04u,0x0000u,0x00000000u},
    {21u,0x09u,0x0000u,0x00000000u},
    {8u,0x04u,0x0000u,0x00000008u},
    {21u,0x09u,0x0000u,0x00000000u},
    {8u,0x04u,0x0000u,0x00000000u},
    {21u,0x1Du,0x0000u,0x00000000u},
    {1u,0x4Du,0x0000u,0x00000000u},
    {4u,0x00u,0x0000u,0x00000000u},
    {3u,0xFBu,0x0000u,0x00000000u},
    {21u,0x0Eu,0x0000u,0x00000000u},
    {23u,0x82u,0x0000u,0x00000000u},
    {24u,0x00u,0x0000u,0x00000000u},
    {2u,0x28u,0x0000u,0x00000000u},
    {5u,0x00u,0xEC28u,0x00000071u},
    {5u,0x00u,0xEC2Au,0x00000068u},
    {5u,0x00u,0xEC48u,0x00000048u},
    {5u,0x00u,0xEC4Au,0x00000038u},
    {5u,0x00u,0xEC44u,0x00000004u},
    {5u,0x00u,0xEC46u,0x00000004u},
    {6u,0x03u,0xEC4Eu,0x00000000u},
    {7u,0x00u,0x0000u,0x00000000u},
    {9u,0x00u,0x0000u,0x00000000u},
    {1u,0x53u,0x0000u,0x00000000u},
    {25u,0x02u,0x0000u,0x00000000u},
    {22u,0x29u,0x0000u,0x00000002u},
    {1u,0x50u,0x0000u,0x00000000u},
    {17u,0x00u,0x0000u,0x00000000u},
    {1u,0x54u,0x0000u,0x00000000u},
    {4u,0x00u,0x0000u,0x00000000u},
    {3u,0xFBu,0x0000u,0x00000000u},
    {21u,0x0Eu,0x0000u,0x00000000u},
    {23u,0x9Eu,0x0000u,0x00000000u},
    {9u,0x00u,0x0000u,0x00000000u},
    {24u,0x00u,0x0000u,0x00000000u},
    {2u,0x2Au,0x0000u,0x00000000u},
    {5u,0x00u,0xEC28u,0x00000018u},
    {5u,0x00u,0xEC2Au,0x00000010u},
    {5u,0x00u,0xEC48u,0x0000005Au},
    {5u,0x00u,0xEC4Au,0x00000036u},
    {5u,0x00u,0xEC44u,0x00000004u},
    {5u,0x00u,0xEC46u,0x00000004u},
    {6u,0x03u,0xEC4Eu,0x00000000u},
    {7u,0x00u,0x0000u,0x00000000u},
    {1u,0x5Bu,0x0000u,0x00000000u},
    {1u,0x61u,0x0000u,0x00000000u},
    {2u,0x7Du,0x0000u,0x00000000u},
    {3u,0x9Eu,0x0000u,0x00000000u},
    {4u,0x00u,0x0000u,0x00000000u},
    {5u,0x00u,0xEC28u,0x000000E4u},
    {5u,0x00u,0xEC2Au,0x000000E0u},
    {5u,0x00u,0xEC48u,0x00000038u},
    {5u,0x00u,0xEC4Au,0x00000046u},
    {5u,0x00u,0xEC44u,0x00000004u},
    {5u,0x00u,0xEC46u,0x00000004u},
    {6u,0x03u,0xEC4Eu,0x00000000u},
    {7u,0x00u,0x0000u,0x00000000u},
    {8u,0x00u,0x0000u,0x00000000u},
    {9u,0x00u,0x0000u,0x00000000u},
    {1u,0x0Au,0x0000u,0x00000000u},
    {1u,0x64u,0x0000u,0x00000000u},
    {12u,0x94u,0x0000u,0x00000000u},
    {4u,0x00u,0x0000u,0x00000000u},
    {13u,0x1Cu,0x0002u,0x00000000u},
    {3u,0xF1u,0x0000u,0x00000000u},
    {5u,0x00u,0xEC28u,0x0000005Eu},
    {5u,0x00u,0xEC2Au,0x00000054u},
    {5u,0x00u,0xEC48u,0x00000050u},
    {5u,0x00u,0xEC4Au,0x00000046u},
    {5u,0x00u,0xEC44u,0x0000000Cu},
    {5u,0x00u,0xEC46u,0x00000010u},
    {6u,0x03u,0xEC4Eu,0x00000000u},
    {7u,0x00u,0x0000u,0x00000000u},
    {9u,0x00u,0x0000u,0x00000000u},
    {1u,0x66u,0x0000u,0x00000000u},
    {12u,0x95u,0x0000u,0x00000000u},
    {4u,0x00u,0x0000u,0x00000000u},
    {13u,0x1Du,0x0002u,0x00000000u},
    {3u,0xF1u,0x0000u,0x00000000u},
    {5u,0x00u,0xEC28u,0x0000005Eu},
    {5u,0x00u,0xEC2Au,0x00000054u},
    {5u,0x00u,0xEC48u,0x00000038u},
    {5u,0x00u,0xEC4Au,0x0000003Eu},
    {5u,0x00u,0xEC44u,0x00000004u},
    {5u,0x00u,0xEC46u,0x00000004u},
    {6u,0x03u,0xEC4Eu,0x00000000u},
    {7u,0x00u,0x0000u,0x00000000u},
    {9u,0x00u,0x0000u,0x00000000u},
    {3u,0xFBu,0x0000u,0x00000000u},
    {4u,0x00u,0x0000u,0x00000000u},
    {5u,0x00u,0xEC28u,0x00000010u},
    {5u,0x00u,0xEC2Au,0x00000000u},
    {5u,0x00u,0xEC48u,0x0000003Eu},
    {5u,0x00u,0xEC4Au,0x00000056u},
    {5u,0x00u,0xEC44u,0x00000000u},
    {5u,0x00u,0xEC46u,0x00000008u},
    {6u,0x03u,0xEC4Eu,0x00000000u},
    {7u,0x00u,0x0000u,0x00000000u},
    {3u,0x84u,0x0000u,0x00000000u},
    {10u,0x84u,0xECECu,0x00000000u},
    {9u,0x00u,0x0000u,0x00000000u},
    {11u,0x00u,0x0000u,0x001FD7A0u},
    {1u,0x10u,0x0000u,0x00000000u},
    {11u,0x00u,0x0000u,0x001DF600u},
};
struct Ps4TransitionEventScript { uint8_t event_index; uint16_t first_op; uint8_t op_count; };
static const struct Ps4TransitionEventScript ps4_transition_event_scripts[8]={
    {0x69u,0u,20u}, /* $0721CC, 150 bytes */
    {0x6Au,20u,17u}, /* $072262, 112 bytes */
    {0x77u,37u,36u}, /* $0728BE, 294 bytes */
    {0x79u,73u,21u}, /* $072A26, 170 bytes */
    {0x83u,94u,15u}, /* $072CDC, 104 bytes */
    {0x97u,109u,14u}, /* $0736EA, 106 bytes */
    {0x98u,123u,14u}, /* $073754, 106 bytes */
    {0x9Eu,137u,16u}, /* $0738D2, 116 bytes */
};
#define PS4_TRANSITION_EVENT_SCRIPT_COUNT 8u
struct Ps4SimpleRunEventCondition { uint8_t run_id; uint8_t flag; uint16_t event_index; uint8_t require_set; };
static const struct Ps4SimpleRunEventCondition ps4_simple_run_event_conditions[18]={
    {0x08u,0x0Du,0x000Cu,0u}, /* $05637E */
    {0x09u,0x12u,0x000Du,1u}, /* $056396 */
    {0x29u,0x71u,0x800Fu,1u}, /* $056BF4 */
    {0x2Cu,0x86u,0x003Du,0u}, /* $056C8E */
    {0x31u,0x80u,0x0043u,0u}, /* $056D3E */
    {0x33u,0xD1u,0x0048u,0u}, /* $056D7C */
    {0x3Du,0x8Bu,0x0053u,0u}, /* $056F94 */
    {0x43u,0x9Fu,0x0058u,0u}, /* $057094 */
    {0x47u,0x9Bu,0x8016u,1u}, /* $057132 */
    {0x49u,0xE2u,0x005Eu,0u}, /* $057184 */
    {0x4Au,0xE3u,0x005Fu,0u}, /* $05719C */
    {0x54u,0xE8u,0x8021u,1u}, /* $05731E */
    {0x56u,0xE0u,0x0099u,0u}, /* $05738E */
    {0x57u,0xC2u,0x009Au,0u}, /* $0573A6 */
    {0x58u,0xC3u,0x009Bu,0u}, /* $0573BE */
    {0x59u,0xC4u,0x009Cu,0u}, /* $0573D6 */
    {0x71u,0x2Eu,0x007Cu,0u}, /* $057476 */
    {0x78u,0xC7u,0x0092u,0u}, /* $057572 */
};
#define PS4_SIMPLE_RUN_EVENT_CONDITION_COUNT 18u
struct Ps4RunEventFlagChainCondition { uint8_t run_id; uint16_t event_index; uint8_t count; uint8_t bank0,flag0,require0,bank1,flag1,require1,bank2,flag2,require2; };
static const struct Ps4RunEventFlagChainCondition ps4_run_event_flag_chain_conditions[39]={
    {0x0Au,0x000Fu,2u,0u,0x09u,1u,0u,0x0Eu,0u,0u,0x00u,0u}, /* $0563AE */
    {0x13u,0x0025u,2u,0u,0x0Fu,0u,0u,0x0Bu,1u,0u,0x00u,0u}, /* $056884 */
    {0x14u,0x8004u,2u,0u,0x30u,0u,0u,0x36u,1u,0u,0x00u,0u}, /* $0568AA */
    {0x15u,0x8005u,2u,0u,0x33u,0u,0u,0x32u,1u,0u,0x00u,0u}, /* $0568D0 */
    {0x17u,0x8006u,2u,0u,0x37u,0u,0u,0x33u,1u,0u,0x00u,0u}, /* $05691C */
    {0x18u,0x0028u,2u,0u,0x32u,0u,1u,0x08u,1u,0u,0x00u,0u}, /* $056942 */
    {0x1Du,0x8009u,2u,0u,0x42u,1u,0u,0x47u,0u,0u,0x00u,0u}, /* $056A12 */
    {0x1Eu,0x800Au,2u,1u,0x09u,1u,0u,0x67u,0u,0u,0x00u,0u}, /* $056A38 */
    {0x20u,0x800Bu,2u,0u,0x68u,0u,0u,0x65u,1u,0u,0x00u,0u}, /* $056A86 */
    {0x26u,0x003Bu,1u,2u,0x18u,0u,0u,0x00u,0u,0u,0x00u,0u}, /* $056B82 */
    {0x27u,0x003Cu,1u,2u,0x18u,1u,0u,0x00u,0u,0u,0x00u,0u}, /* $056B9A */
    {0x2Fu,0x0041u,2u,0u,0x41u,1u,0u,0x48u,0u,0u,0x00u,0u}, /* $056CF2 */
    {0x30u,0x0027u,2u,0u,0x11u,1u,0u,0x13u,0u,0u,0x00u,0u}, /* $056D18 */
    {0x32u,0x8011u,2u,0u,0x89u,0u,0u,0x83u,1u,0u,0x00u,0u}, /* $056D56 */
    {0x34u,0x801Cu,3u,0u,0xD6u,0u,0u,0xD5u,1u,0u,0xD3u,1u}, /* $056D94 */
    {0x38u,0x8017u,2u,0u,0xA1u,0u,0u,0x9Eu,1u,0u,0x00u,0u}, /* $056EAE */
    {0x3Bu,0x801Au,2u,1u,0x0Du,1u,0u,0xC5u,0u,0u,0x00u,0u}, /* $056F48 */
    {0x3Cu,0x0050u,2u,0u,0xC5u,1u,0u,0xC6u,0u,0u,0x00u,0u}, /* $056F6E */
    {0x3Eu,0x0054u,2u,0u,0x92u,0u,0u,0x9Eu,0u,0u,0x00u,0u}, /* $056FAC */
    {0x3Fu,0x0055u,3u,0u,0x92u,1u,0u,0xA4u,0u,0u,0x9Eu,0u}, /* $056FD2 */
    {0x41u,0x0057u,3u,0u,0x93u,1u,0u,0xA5u,0u,0u,0x9Eu,0u}, /* $05703A */
    {0x42u,0x8015u,2u,0u,0x98u,1u,0u,0x99u,0u,0u,0x00u,0u}, /* $05706E */
    {0x45u,0x005Au,2u,0u,0xA6u,0u,1u,0x0Cu,1u,0u,0x00u,0u}, /* $0570E6 */
    {0x46u,0x005Du,2u,0u,0x9Bu,0u,0u,0xA6u,1u,0u,0x00u,0u}, /* $05710C */
    {0x4Bu,0x0064u,2u,0u,0xE5u,0u,0u,0xD4u,1u,0u,0x00u,0u}, /* $0571B4 */
    {0x4Cu,0x0065u,2u,0u,0xE6u,0u,0u,0xD2u,1u,0u,0x00u,0u}, /* $0571DA */
    {0x4Eu,0x0063u,2u,0u,0xE4u,1u,0u,0xE1u,0u,0u,0x00u,0u}, /* $057204 */
    {0x4Fu,0x801Du,2u,0u,0xD6u,1u,0u,0xD8u,0u,0u,0x00u,0u}, /* $05722A */
    {0x50u,0x801Fu,2u,0u,0xD9u,1u,0u,0xDAu,0u,0u,0x00u,0u}, /* $057250 */
    {0x5Au,0x009Du,2u,0u,0xC8u,0u,1u,0x0Du,1u,0u,0x00u,0u}, /* $0573EE */
    {0x5Bu,0x009Eu,1u,0u,0x0Cu,0u,0u,0x00u,0u,0u,0x00u,0u}, /* $057414 */
    {0x72u,0x007Eu,2u,0u,0x2Fu,1u,0u,0x3Eu,0u,0u,0x00u,0u}, /* $05748E */
    {0x73u,0x0089u,2u,0u,0xBAu,0u,0u,0xAFu,1u,0u,0x00u,0u}, /* $0574B4 */
    {0x74u,0x008Au,2u,0u,0xB2u,0u,0u,0xB1u,1u,0u,0x00u,0u}, /* $0574DA */
    {0x75u,0x008Du,2u,0u,0xB4u,0u,0u,0xB3u,1u,0u,0x00u,0u}, /* $057500 */
    {0x76u,0x008Eu,2u,0u,0xB5u,0u,0u,0xB4u,1u,0u,0x00u,0u}, /* $057526 */
    {0x77u,0x0091u,2u,0u,0xBBu,0u,0u,0xB6u,1u,0u,0x00u,0u}, /* $05754C */
    {0x7Au,0x0095u,1u,2u,0x1Bu,1u,0u,0x00u,0u,0u,0x00u,0u}, /* $0575C4 */
    {0x7Cu,0x00A0u,1u,0u,0x15u,0u,0u,0x00u,0u,0u,0x00u,0u}, /* $05760A */
};
#define PS4_RUN_EVENT_FLAG_CHAIN_CONDITION_COUNT 39u
struct Ps4RunEventFlagCoordEqCondition { uint8_t run_id; uint16_t event_index; uint8_t count; uint8_t bank0,flag0,require0,bank1,flag1,require1,bank2,flag2,require2; uint16_t x,y; };
static const struct Ps4RunEventFlagCoordEqCondition ps4_run_event_flag_coord_eq_conditions[9]={
    {0x23u,0x800Du,0u,0u,0x00u,0u,0u,0x00u,0u,0u,0x00u,0u,0xFFFFu,0x02F0u}, /* $056B3A */
    {0x24u,0x800Du,0u,0u,0x00u,0u,0u,0x00u,0u,0u,0x00u,0u,0xFFFFu,0x0370u}, /* $056B52 */
    {0x25u,0x800Du,0u,0u,0x00u,0u,0u,0x00u,0u,0u,0x00u,0u,0xFFFFu,0x0120u}, /* $056B6A */
    {0x28u,0x800Eu,3u,0u,0x71u,0u,0u,0x70u,1u,0u,0x72u,1u,0xFFFFu,0x02F0u}, /* $056BB2 */
    {0x2Du,0x003Eu,1u,0u,0x87u,0u,0u,0x00u,0u,0u,0x00u,0u,0xFFFFu,0x0200u}, /* $056CA6 */
    {0x2Eu,0x003Fu,1u,0u,0x83u,0u,0u,0x00u,0u,0u,0x00u,0u,0xFFFFu,0x00D0u}, /* $056CCC */
    {0x37u,0x004Eu,1u,0u,0x9Eu,0u,0u,0x00u,0u,0u,0x00u,0u,0xFFFFu,0x0100u}, /* $056E88 */
    {0x3Au,0x8019u,1u,0u,0xC1u,0u,0u,0x00u,0u,0u,0x00u,0u,0x0770u,0x09B0u}, /* $056F18 */
    {0x40u,0x0056u,2u,0u,0x93u,0u,0u,0x9Eu,0u,0u,0x00u,0u,0xFFFFu,0x00F0u}, /* $057006 */
};
#define PS4_RUN_EVENT_FLAG_COORD_EQ_CONDITION_COUNT 9u
struct Ps4RunEventFlagCoordRangeCondition { uint8_t run_id; uint16_t event_index; uint8_t flag_count; uint8_t bank0,flag0,require0,bank1,flag1,require1,bank2,flag2,require2; uint8_t cmp_count; uint8_t axis0,op0; uint16_t value0; uint8_t axis1,op1; uint16_t value1; uint8_t axis2,op2; uint16_t value2; uint8_t axis3,op3; uint16_t value3; };
static const struct Ps4RunEventFlagCoordRangeCondition ps4_run_event_flag_coord_range_conditions[11]={
    {0x16u,0x000Du,1u,0u,0x12u,0u,0u,0x00u,0u,0u,0x00u,0u,1u,1u,1u,0x0280u,0u,0u,0x0000u,0u,0u,0x0000u,0u,0u,0x0000u}, /* $0568F6 */
    {0x19u,0x002Eu,1u,0u,0x62u,0u,0u,0x00u,0u,0u,0x00u,0u,1u,0u,4u,0x03C0u,0u,0u,0x0000u,0u,0u,0x0000u,0u,0u,0x0000u}, /* $056968 */
    {0x1Bu,0x002Bu,2u,0u,0x44u,0u,1u,0x0Au,1u,0u,0x00u,0u,1u,1u,4u,0x01C0u,0u,0u,0x0000u,0u,0u,0x0000u,0u,0u,0x0000u}, /* $0569B6 */
    {0x21u,0x800Du,0u,0u,0x00u,0u,0u,0x00u,0u,0u,0x00u,0u,3u,0u,4u,0x01E0u,0u,1u,0x01F0u,1u,0u,0x0120u,0u,0u,0x0000u}, /* $056AAC */
    {0x2Au,0x002Fu,1u,0u,0x69u,0u,0u,0x00u,0u,0u,0x00u,0u,3u,1u,0u,0x01B0u,0u,4u,0x01E0u,0u,1u,0x01F0u,0u,0u,0x0000u}, /* $056C0C */
    {0x2Bu,0x8010u,2u,0u,0x84u,1u,0u,0x82u,0u,0u,0x00u,0u,3u,1u,0u,0x0520u,0u,4u,0x01A0u,0u,1u,0x01B0u,0u,0u,0x0000u}, /* $056C46 */
    {0x36u,0x0047u,2u,0u,0x9Cu,0u,0u,0x9Bu,1u,0u,0x00u,0u,3u,1u,1u,0x00E0u,0u,4u,0x0B80u,0u,1u,0x0BB0u,0u,0u,0x0000u}, /* $056E40 */
    {0x39u,0x8014u,1u,0u,0x97u,0u,0u,0x00u,0u,0u,0x00u,0u,4u,0u,4u,0x01E0u,0u,1u,0x0210u,1u,4u,0x01D0u,1u,1u,0x01F0u}, /* $056ED4 */
    {0x44u,0x0059u,1u,0u,0x9Au,0u,0u,0x00u,0u,0u,0x00u,0u,3u,0u,4u,0x01D0u,0u,1u,0x0220u,1u,1u,0x0220u,0u,0u,0x0000u}, /* $0570AC */
    {0x48u,0x8018u,1u,0u,0x9Du,0u,0u,0x00u,0u,0u,0x00u,0u,3u,0u,4u,0x01E0u,0u,1u,0x01F0u,1u,1u,0x0180u,0u,0u,0x0000u}, /* $05714A */
    {0x79u,0x0094u,1u,2u,0x1Bu,0u,0u,0x00u,0u,0u,0x00u,0u,3u,1u,1u,0x0240u,0u,4u,0x01E0u,0u,1u,0x0270u,0u,0u,0x0000u}, /* $05758A */
};
#define PS4_RUN_EVENT_FLAG_COORD_RANGE_CONDITION_COUNT 11u
static const uint16_t ps4_run_event_condition_binding[128]={
    0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0x0100u,0x0101u,0x0200u,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,
    0xFFFFu,0xFFFFu,0xFFFFu,0x0201u,0x0202u,0x0203u,0x0400u,0x0204u,0x0205u,0x0401u,0xFFFFu,0x0402u,0xFFFFu,0x0206u,0x0207u,0xFFFFu,
    0x0208u,0x0403u,0xFFFFu,0x0300u,0x0301u,0x0302u,0x0209u,0x020Au,0x0303u,0x0102u,0x0404u,0x0405u,0x0103u,0x0304u,0x0305u,0x020Bu,
    0x020Cu,0x0104u,0x020Du,0x0105u,0x020Eu,0xFFFFu,0x0406u,0x0306u,0x020Fu,0x0407u,0x0307u,0x0210u,0x0211u,0x0106u,0x0212u,0x0213u,
    0x0308u,0x0214u,0x0215u,0x0107u,0x0408u,0x0216u,0x0217u,0x0108u,0x0409u,0x0109u,0x010Au,0x0218u,0x0219u,0xFFFFu,0x021Au,0x021Bu,
    0x021Cu,0xFFFFu,0xFFFFu,0xFFFFu,0x010Bu,0xFFFFu,0x010Cu,0x010Du,0x010Eu,0x010Fu,0x021Du,0x021Eu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,
    0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,0xFFFFu,
    0xFFFFu,0x0110u,0x021Fu,0x0220u,0x0221u,0x0222u,0x0223u,0x0224u,0x0111u,0x040Au,0x0225u,0xFFFFu,0x0226u,0xFFFFu,0xFFFFu,0xFFFFu,
};
#define PS4_EVENT_PTRS_SOURCE_COUNT 0xA1u
#endif
