/* Auto-generated from verified PSIV US ROM by generate_cutscene_scripts.py. */
#ifndef PS4_GENERATED_CUTSCENE_SCRIPTS_V0982_H
#define PS4_GENERATED_CUTSCENE_SCRIPTS_V0982_H
#define PS4_CUTOP_SOUND 1u
#define PS4_CUTOP_DIALOGUE 2u
#define PS4_CUTOP_FLAG_SET 3u
#define PS4_CUTOP_RAM8 4u
#define PS4_CUTOP_RAM16 5u
#define PS4_CUTOP_ADD32 6u
#define PS4_CUTOP_REFRESH_MAP 7u
#define PS4_CUTOP_FADE_IN 8u
#define PS4_CUTOP_PREP 9u
struct Ps4CutsceneOp { uint8_t opcode; uint8_t arg8; uint16_t addr; uint32_t value; };
static const struct Ps4CutsceneOp ps4_cutscene_ops[27]={
    {1u,0xFBu,0x0000u,0x00000000u},
    {9u,0x00u,0x0000u,0x00000000u},
    {1u,0x9Fu,0x0000u,0x00000000u},
    {8u,0x00u,0x0000u,0x00000000u},
    {4u,0x01u,0xECFDu,0x00000000u},
    {2u,0x17u,0x0000u,0x00000000u},
    {3u,0x09u,0x0000u,0x00000000u},
    {1u,0xFBu,0x0000u,0x00000000u},
    {4u,0x00u,0xECECu,0x00000000u},
    {9u,0x00u,0x0000u,0x00000000u},
    {8u,0x00u,0x0000u,0x00000000u},
    {1u,0xABu,0x0000u,0x00000000u},
    {4u,0x01u,0xECFDu,0x00000000u},
    {2u,0x66u,0x0000u,0x00000000u},
    {1u,0xFBu,0x0000u,0x00000000u},
    {5u,0x00u,0xEC28u,0x00000024u},
    {5u,0x00u,0xEC2Au,0x0000002Bu},
    {5u,0x00u,0xEC48u,0x0000003Cu},
    {5u,0x00u,0xEC4Au,0x00000014u},
    {5u,0x00u,0xEC44u,0x00000000u},
    {5u,0x00u,0xEC46u,0x00000000u},
    {7u,0x00u,0x0000u,0x00000000u},
    {8u,0x00u,0x0000u,0x00000000u},
    {1u,0x97u,0x0000u,0x00000000u},
    {4u,0x97u,0xECECu,0x00000000u},
    {6u,0x00u,0xF438u,0x000001F4u},
    {3u,0x10u,0x0000u,0x00000000u},
};
struct Ps4CutsceneScript { uint8_t cutscene_index; uint16_t first_op; uint8_t op_count; };
static const struct Ps4CutsceneScript ps4_cutscene_scripts[2]={
    {0x01u,0u,9u}, /* $073EE0, 66 bytes */
    {0x02u,9u,18u}, /* $073F22, 122 bytes */
};
#define PS4_CUTSCENE_SCRIPT_COUNT 2u
#define PS4_CUTSCENE_SOURCE_COUNT 0x22u
#endif
