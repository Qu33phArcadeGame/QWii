// gfx.h - made by tools/make_gba_assets.py from the DS art (do not edit)
#pragma once
#include <stdint.h>
#define FIELD_W 176
#define FIELD_H 720
#define MM_X0 30
#define MM_W 24
#define MM_H 160
#define LOGO_W 111
#define LOGO_H 40
#define COIN_W 18
#define COIN_H 18
#define MACH_W 171
#define MACH_H 160
#define SYM_W 22
#define REEL_X0 51
#define REEL_X1 84
#define REEL_X2 116
#define REEL_Y 67
extern const uint8_t coin8[COIN_W * COIN_H];      // the coin in the menus' palette
extern const uint8_t mach8[MACH_W * MACH_H];      // the slot screen: 0 = see-through, else palette 64-255
extern const uint8_t sym8[7][SYM_W * SYM_W];
extern const uint8_t scoin8[COIN_W * COIN_H];
extern const uint16_t slot_pal[192];
#define ARC_W 44
#define ARC_H 56
extern const uint8_t arc8[9][ARC_W * ARC_H];   // the arcade cabinets (menus' palette), in the DS's game order
#define MINIT_W 120
#define MINIT_H 160
#define MINIT_Y0 40
#define MINIT_S 0.465f
extern const uint8_t minit8[MINIT_W * MINIT_H];   // Mini Qu33ph's table, palette 64-255
extern const uint16_t minit_pal[192];
extern const uint8_t mm_green[1024];
extern const uint16_t mm_green_pal[16];
extern const uint8_t mm_pink[1024];
extern const uint16_t mm_pink_pal[16];
extern const uint8_t mm_yellow[1024];
extern const uint16_t mm_yellow_pal[16];
extern const uint8_t mm_blue[1024];
extern const uint16_t mm_blue_pal[16];
extern const uint8_t mm_case[1024];
extern const uint16_t mm_case_pal[16];
extern const uint8_t mm_chair[512];
extern const uint16_t mm_chair_pal[16];
#define BB_W 128
#define BB_H 160
extern const uint8_t bb_field[3][BB_W * BB_H];   // Qu33ph-Ball's machines, palette 64-255
extern const uint16_t bb_pal[3][192];
extern const uint8_t fd8[240 * 160];          // Fidget's field (on its side), palette 64-255
extern const uint16_t fd_pal[192];
extern const uint8_t fd_spin0[2048];
extern const uint16_t fd_spin0_pal[16];
extern const uint8_t fd_spin1[2048];
extern const uint16_t fd_spin1_pal[16];
extern const uint8_t fd_ball[512];
extern const uint16_t fd_ball_pal[16];
#define BWL_W 160
#define BWL_Y0 86
#define BWL_S 0.620155f
extern const uint8_t bwl8[160 * 160];   // Bowling's lane, palette 64-255
extern const uint16_t bwl_pal[192];
extern const uint8_t bw_pin0[1024];
extern const uint16_t bw_pin0_pal[16];
extern const uint8_t bw_pin1[1024];
extern const uint16_t bw_pin1_pal[16];
extern const uint8_t bw_pin2[1024];
extern const uint16_t bw_pin2_pal[16];
extern const uint8_t bw_mega[256];
extern const uint16_t bw_mega_pal[16];
extern const uint8_t bw_spin0[256];
extern const uint16_t bw_spin0_pal[16];
extern const uint8_t bw_q0[64];
extern const uint16_t bw_q0_pal[16];
extern const uint8_t bw_spin1[256];
extern const uint16_t bw_spin1_pal[16];
extern const uint8_t bw_q1[64];
extern const uint16_t bw_q1_pal[16];
extern const uint8_t bw_spin2[256];
extern const uint16_t bw_spin2_pal[16];
extern const uint8_t bw_q2[64];
extern const uint16_t bw_q2_pal[16];
extern const uint8_t bq_0[128];
extern const uint16_t bq_0_pal[16];
extern const uint8_t bq_1[128];
extern const uint16_t bq_1_pal[16];
extern const uint8_t bq_2[128];
extern const uint16_t bq_2_pal[16];
#define STW_W 160
#define STW_H 204
extern const uint8_t stw8[STW_W * STW_H];   // Stack's window backdrop, palette 64-255
extern const uint16_t stw_pal[192];
extern const uint8_t st_v0[1024];
extern const uint16_t st_v0_pal[16];
extern const uint8_t st_h0[1024];
extern const uint16_t st_h0_pal[16];
extern const uint8_t st_v1[1024];
extern const uint16_t st_v1_pal[16];
extern const uint8_t st_h1[1024];
extern const uint16_t st_h1_pal[16];
extern const uint8_t st_v2[1024];
extern const uint16_t st_v2_pal[16];
extern const uint8_t st_h2[1024];
extern const uint16_t st_h2_pal[16];
#define FLW 548
#define FL_Y0 90
extern const uint8_t fl8[4][160 * FLW];   // Flip's backdrop (4 copies, shifted 0-3 px), palette 64-255
extern const uint16_t fl_pal[192];
extern const uint8_t fl_p0[1024];
extern const uint16_t fl_p0_pal[16];
extern const uint8_t fl_p1[1024];
extern const uint16_t fl_p1_pal[16];
extern const uint8_t fl_p2[1024];
extern const uint16_t fl_p2_pal[16];
extern const uint8_t fl_m[512];
extern const uint16_t fl_m_pal[16];
#define DZ_W 148
#define DZ_Y0 56
extern const uint8_t dz8[DZ_W * 160];   // Dozer's cabinet, palette 64-255
extern const uint16_t dz_pal8[192];
#define DZC_MIN 2
#define DZC_MAX 14
extern const uint8_t *const dzcoin[13];   // coins 2-14 across (82% as tall), 0 see-through
extern const uint8_t dz_m0[128];
extern const uint16_t dz_m0_pal[16];
extern const uint8_t dz_m1[128];
extern const uint16_t dz_m1_pal[16];
extern const uint8_t dz_m2[128];
extern const uint16_t dz_m2_pal[16];
#define PBT_W 152
#define PBT_Y0 25
#define PBT_S 0.581818f
extern const uint8_t pb8[152 * 160];   // Pinball's table, palette 64-255
extern const uint16_t pb_pal8[192];
extern const uint8_t pb_ball0[256];
extern const uint16_t pb_ball0_pal[16];
extern const uint8_t pb_ball1[256];
extern const uint16_t pb_ball1_pal[16];
extern const uint8_t pb_fl[256];
extern const uint16_t pb_fl_pal[16];
extern const uint8_t pb_flm[256];
extern const uint16_t pb_flm_pal[16];
// Jump: runner k pose p (0 stand, 1 run, 2 jump) jr_k_p (32x32); standing markers jo_k (16x32);
// flock jf_0-11 (16x16: 2 eggs, 5 peeking, 5 chicks); the MEGA pad jmega (32x32)
extern const uint8_t jr_0_0[512];
extern const uint16_t jr_0_0_pal[16];
extern const uint8_t jr_0_1[512];
extern const uint16_t jr_0_1_pal[16];
extern const uint8_t jr_0_2[512];
extern const uint16_t jr_0_2_pal[16];
extern const uint8_t jr_1_0[512];
extern const uint16_t jr_1_0_pal[16];
extern const uint8_t jr_1_1[512];
extern const uint16_t jr_1_1_pal[16];
extern const uint8_t jr_1_2[512];
extern const uint16_t jr_1_2_pal[16];
extern const uint8_t jr_2_0[512];
extern const uint16_t jr_2_0_pal[16];
extern const uint8_t jr_2_1[512];
extern const uint16_t jr_2_1_pal[16];
extern const uint8_t jr_2_2[512];
extern const uint16_t jr_2_2_pal[16];
extern const uint8_t jr_3_0[512];
extern const uint16_t jr_3_0_pal[16];
extern const uint8_t jr_3_1[512];
extern const uint16_t jr_3_1_pal[16];
extern const uint8_t jr_3_2[512];
extern const uint16_t jr_3_2_pal[16];
extern const uint8_t jr_4_0[512];
extern const uint16_t jr_4_0_pal[16];
extern const uint8_t jr_4_1[512];
extern const uint16_t jr_4_1_pal[16];
extern const uint8_t jr_4_2[512];
extern const uint16_t jr_4_2_pal[16];
extern const uint8_t jr_5_0[512];
extern const uint16_t jr_5_0_pal[16];
extern const uint8_t jr_5_1[512];
extern const uint16_t jr_5_1_pal[16];
extern const uint8_t jr_5_2[512];
extern const uint16_t jr_5_2_pal[16];
extern const uint8_t jr_6_0[512];
extern const uint16_t jr_6_0_pal[16];
extern const uint8_t jr_6_1[512];
extern const uint16_t jr_6_1_pal[16];
extern const uint8_t jr_6_2[512];
extern const uint16_t jr_6_2_pal[16];
extern const uint8_t jo_0[256];
extern const uint16_t jo_0_pal[16];
extern const uint8_t jo_1[256];
extern const uint16_t jo_1_pal[16];
extern const uint8_t jo_2[256];
extern const uint16_t jo_2_pal[16];
extern const uint8_t jf_0[128];
extern const uint16_t jf_0_pal[16];
extern const uint8_t jf_1[128];
extern const uint16_t jf_1_pal[16];
extern const uint8_t jf_2[128];
extern const uint16_t jf_2_pal[16];
extern const uint8_t jf_3[128];
extern const uint16_t jf_3_pal[16];
extern const uint8_t jf_4[128];
extern const uint16_t jf_4_pal[16];
extern const uint8_t jf_5[128];
extern const uint16_t jf_5_pal[16];
extern const uint8_t jf_6[128];
extern const uint16_t jf_6_pal[16];
extern const uint8_t jf_7[128];
extern const uint16_t jf_7_pal[16];
extern const uint8_t jf_8[128];
extern const uint16_t jf_8_pal[16];
extern const uint8_t jf_9[128];
extern const uint16_t jf_9_pal[16];
extern const uint8_t jf_10[128];
extern const uint16_t jf_10_pal[16];
extern const uint8_t jf_11[128];
extern const uint16_t jf_11_pal[16];
extern const uint8_t jmega[512];
extern const uint16_t jmega_pal[16];
extern const uint8_t mm_shadow[256];          // 32x16, colour 1
// PlinQu33ph markers: red, green 32x32; blue 64x32 (4bpp tiles)
extern const uint8_t pm_red[512];
extern const uint16_t pm_red_pal[16];
extern const uint8_t pm_green[512];
extern const uint16_t pm_green_pal[16];
extern const uint8_t pm_blue[1024];
extern const uint16_t pm_blue_pal[16];
extern const uint8_t field8[FIELD_W * FIELD_H];   // palette 64-255
extern const uint16_t field_pal[192];
extern const uint8_t mini8[MM_W * MM_H];
extern const uint8_t logo8[LOGO_W * LOGO_H];      // 0 = see-through, else palette 64-255
extern const uint16_t logo_pal[192];
// 64x64 4bpp sprite tiles and their 16-colour palettes
extern const uint8_t obj_mk_green[2048];
extern const uint16_t obj_mk_green_pal[16];
extern const uint8_t obj_mk_red[2048];
extern const uint16_t obj_mk_red_pal[16];
extern const uint8_t obj_mk_blue[2048];
extern const uint16_t obj_mk_blue_pal[16];
extern const uint8_t obj_chair[2048];
extern const uint16_t obj_chair_pal[16];
extern const uint8_t glow_mk_green[2048];
extern const uint8_t glow_mk_red[2048];
extern const uint8_t glow_mk_blue[2048];
extern const uint16_t font_rows[1330];
extern const uint16_t fonts_rows[950];
extern const uint8_t font_w[95];
extern const uint8_t fonts_w[95];
#define FONT_H 14
#define FONTS_H 10
