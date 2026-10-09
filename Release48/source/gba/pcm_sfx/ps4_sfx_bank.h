#ifndef PS4_SFX_BANK_H
#define PS4_SFX_BANK_H

#include <stdint.h>

#define PS4_SFX_FIRST_ID 0xB5u
#define PS4_SFX_LAST_ID 0xFAu
#define PS4_SFX_COUNT 70u
#define PS4_SFX_BANK_BYTES 1881126u

typedef struct { uint32_t offset; uint32_t length; uint8_t flags; } ps4_sfx_desc_t;
#define PS4_SFX_FLAG_SPECIAL_LOOP 1u
#define PS4_SFX_FLAG_SILENT 2u

static const ps4_sfx_desc_t ps4_sfx_descs[PS4_SFX_COUNT] = {
    {0u,3679u,0u}, /* $B5 rod */
    {3679u,5256u,0u}, /* $B6 shot */
    {8935u,5256u,0u}, /* $B7 slasher */
    {14191u,2102u,0u}, /* $B8 attack_miss */
    {16293u,27331u,0u}, /* $B9 enemy_killed */
    {43624u,4730u,0u}, /* $BA enemy_attack1 */
    {48354u,24178u,0u}, /* $BB tech_cast */
    {72532u,14717u,0u}, /* $BC buff_cast */
    {87249u,17870u,0u}, /* $BD heal_tech_cast */
    {105119u,11563u,0u}, /* $BE foi */
    {116682u,12089u,0u}, /* $BF legeon */
    {128771u,39946u,0u}, /* $C0 megid */
    {168717u,14717u,0u}, /* $C1 phonon */
    {183434u,21024u,0u}, /* $C2 fire_breath */
    {204458u,24178u,0u}, /* $C3 efess */
    {228636u,17345u,0u}, /* $C4 moonshad */
    {245981u,17345u,0u}, /* $C5 laser_attack */
    {263326u,13140u,0u}, /* $C6 zan */
    {276466u,4205u,0u}, /* $C7 gra */
    {280671u,27857u,0u}, /* $C8 vol */
    {308528u,22075u,0u}, /* $C9 saner */
    {330603u,28908u,0u}, /* $CA rimit */
    {359511u,33638u,0u}, /* $CB brose */
    {393149u,27331u,0u}, /* $CC res */
    {420480u,38369u,0u}, /* $CD recovery */
    {458849u,25754u,0u}, /* $CE tandle */
    {484603u,30485u,0u}, /* $CF android_skill_implant */
    {515088u,23652u,0u}, /* $D0 rifle */
    {538740u,15242u,0u}, /* $D1 sleep_gas */
    {553982u,5782u,0u}, /* $D2 eliminat */
    {559764u,2102u,0u}, /* $D3 spark */
    {561866u,18922u,0u}, /* $D4 war_cry */
    {580788u,10512u,0u}, /* $D5 mole_attack */
    {591300u,21550u,0u}, /* $D6 mech_enemy_alarm */
    {612850u,7358u,0u}, /* $D7 enemy_attack3 */
    {620208u,16294u,0u}, /* $D8 enemy_attack4 */
    {636502u,43625u,0u}, /* $D9 fusion */
    {680127u,14717u,0u}, /* $DA enemy_attack5 */
    {694844u,9986u,0u}, /* $DB alarm */
    {704830u,16294u,0u}, /* $DC deban */
    {721124u,35741u,0u}, /* $DD grave_opening */
    {756865u,35741u,0u}, /* $DE teleport */
    {792606u,29434u,0u}, /* $DF stairs */
    {822040u,25229u,0u}, /* $E0 riding_elevator */
    {847269u,9461u,0u}, /* $E1 chest_opened */
    {856730u,40997u,0u}, /* $E2 door_opened */
    {897727u,38369u,0u}, /* $E3 spaceship_propelled */
    {936096u,67277u,0u}, /* $E4 power_down */
    {1003373u,5782u,0u}, /* $E5 elevator_open */
    {1009155u,33113u,0u}, /* $E6 barrier_broken */
    {1042268u,126144u,0u}, /* $E7 conveyor_belt */
    {1168412u,7358u,0u}, /* $E8 claw */
    {1175770u,15242u,0u}, /* $E9 unused1 */
    {1191012u,31010u,0u}, /* $EA black_wave */
    {1222022u,26280u,0u}, /* $EB enemy_spell_cast */
    {1248302u,60444u,0u}, /* $EC lightning */
    {1308746u,33638u,0u}, /* $ED another_gate */
    {1342384u,22601u,0u}, /* $EE ice_broken */
    {1364985u,16819u,0u}, /* $EF falling_into_hole */
    {1381804u,32062u,0u}, /* $F0 telepipe */
    {1413866u,24178u,0u}, /* $F1 souvenir */
    {1438044u,1577u,0u}, /* $F2 moving_cursor */
    {1439621u,8935u,0u}, /* $F3 selection */
    {1448556u,45202u,0u}, /* $F4 surprise */
    {1493758u,5782u,0u}, /* $F5 sword */
    {1499540u,1577u,2u}, /* $F6 nothing */
    {1501117u,1577u,2u}, /* $F7 unused2 */
    {1502694u,126144u,1u}, /* $F8 spaceship_radar */
    {1628838u,126144u,1u}, /* $F9 land_rover */
    {1754982u,126144u,1u}, /* $FA hydrofoil */
};

#endif
