/*
 * AMLOGIC Audio/Video streaming port driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:  Tim Yao <timyao@amlogic.com>
 *
 */

#include <linux/kernel.h>
#include <linux/vout/vinfo.h>

#include "vframe.h"
#include "video.h"
#include "vpp.h"

/* vpp filter coefficients */
#define COEF_BICUBIC         0
#define COEF_3POINT_TRIANGLE 1
#define COEF_4POINT_TRIANGLE 2
#define COEF_BILINEAR        3

const u32 vpp_filter_coefs_bicubic_sharp[] = 
{
    3,
    33 | 0x8000,
//    0x01f80090, 0x01f80100, 0xff7f0200, 0xfe7f0300,
    0x01fa008c, 0x01fa0100, 0xff7f0200, 0xfe7f0300,
	0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
	0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
	0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
	0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
	0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	0xf84848f8
};

const u32 vpp_filter_coefs_bicubic[] = {
    4,
    33,
    0x00800000, 0x007f0100, 0xff7f0200, 0xfe7f0300,
    0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
    0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
    0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
    0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
    0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
    0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
    0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
    0xf84848f8
};

const u32 vpp_filter_coefs_bilinear[] = {
    4,
    33,
    0x00800000, 0x007e0200, 0x007c0400, 0x007a0600,
    0x00780800, 0x00760a00, 0x00740c00, 0x00720e00,
    0x00701000, 0x006e1200, 0x006c1400, 0x006a1600,
    0x00681800, 0x00661a00, 0x00641c00, 0x00621e00,
    0x00602000, 0x005e2200, 0x005c2400, 0x005a2600,
    0x00582800, 0x00562a00, 0x00542c00, 0x00522e00,
    0x00503000, 0x004e3200, 0x004c3400, 0x004a3600,
    0x00483800, 0x00463a00, 0x00443c00, 0x00423e00,
    0x00404000
};

const u32 vpp_filter_coefs_3point_triangle[] = {
    3,
    33,
    0x40400000, 0x3f400100, 0x3d410200, 0x3c410300,
    0x3a420400, 0x39420500, 0x37430600, 0x36430700,
    0x35430800, 0x33450800, 0x32450900, 0x31450a00,
    0x30450b00, 0x2e460c00, 0x2d460d00, 0x2c470d00,
    0x2b470e00, 0x29480f00, 0x28481000, 0x27481100,
    0x26491100, 0x25491200, 0x24491300, 0x234a1300,
    0x224a1400, 0x214a1500, 0x204a1600, 0x1f4b1600,
    0x1e4b1700, 0x1d4b1800, 0x1c4c1800, 0x1b4c1900,
    0x1a4c1a00
};

const u32 vpp_filter_coefs_4point_triangle[] = {
    4,
    33,
    0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
    0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
    0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
    0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
    0x18382808, 0x18382808, 0x17372909, 0x17372909,
    0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
    0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
    0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
    0x10303010
};

static const u32 *filter_table[] = {
    vpp_filter_coefs_bicubic,
    vpp_filter_coefs_3point_triangle,
    vpp_filter_coefs_4point_triangle,
    vpp_filter_coefs_bilinear
};

static u32 vpp_wide_mode;
static s32 vpp_zoom_center_x, vpp_zoom_center_y;
static u32 video_layer_top, video_layer_left, video_layer_width, video_layer_height;

#define ZOOM_BITS       18
#define PHASE_BITS      8

/*
 *   when ratio for Y is 1:1
 *   Line #   In(P)   In(I)       Out(P)      Out(I)            Out(P)      Out(I)
 *   0        P_Y     IT_Y        P_Y          IT_Y
 *   1                                                          P_Y         IT_Y
 *   2                IB_Y                     IB_Y
 *   3                                                                      IB_Y
 *   4        P_Y     IT_Y        P_Y          IT_Y
 *   5                                                          P_Y         IT_Y
 *   6                IB_Y                     IB_Y
 *   7                                                                      IB_Y
 *   8        P_Y     IT_Y        P_Y          IT_Y
 *   9                                                          P_Y         IT_Y
 *  10                IB_Y                     IB_Y
 *  11                                                                      IB_Y
 *  12        P_Y     IT_Y        P_Y          IT_Y
 *                                                              P_Y         IT_Y
 */

/* The table data sequence here is arranged according to f2v_vphase_type_t enum,
 *  IT2IT, IB2IB, T2IB, IB2IT, P2IT, P2IB, IT2P, IB2P, P2P
 */
static const u8 f2v_420_in_pos[F2V_TYPE_MAX] =
    { 0, 2, 0, 2, 0, 0, 0, 2, 0 };
static const u8 f2v_420_out_pos1[F2V_TYPE_MAX] =
    { 0, 2, 2, 0, 0, 2, 0, 0, 0 };
static const u8 f2v_420_out_pos2[F2V_TYPE_MAX] =
    { 1, 3, 3, 1, 1, 3, 1, 1, 1 };

static void f2v_get_vertical_phase(u32 zoom_ratio,
                                   u32 phase_adj,
                                   f2v_vphase_t vphase[F2V_TYPE_MAX],
                                   u32 interlace)
{
    f2v_vphase_type_t type;
    s32 offset_in, offset_out;
    s32 phase;
    const u8 *f2v_420_out_pos;
    
    if ((interlace == 0) && (zoom_ratio > (1 << ZOOM_BITS)))
        f2v_420_out_pos = f2v_420_out_pos2;
    else
        f2v_420_out_pos = f2v_420_out_pos1;

    for (type = F2V_IT2IT; type < F2V_TYPE_MAX; type++) {
        offset_in = f2v_420_in_pos[type] << PHASE_BITS;
        offset_out =
            (f2v_420_out_pos[type] * zoom_ratio) >> (ZOOM_BITS -
                                                     PHASE_BITS);

        if (offset_in > offset_out) {
            vphase[type].repeat_skip = -1;     /* repeat line */
            vphase[type].phase =
                ((4 << PHASE_BITS) + offset_out - offset_in) >> 2;

        } else {
            vphase[type].repeat_skip = 0;      /* skip line */

            while ((offset_in + (4 << PHASE_BITS)) <= offset_out) {
                vphase[type].repeat_skip++;
                offset_in += 4 << PHASE_BITS;
            }

            vphase[type].phase = (offset_out - offset_in) >> 2;
        }

        phase = vphase[type].phase + phase_adj;

        if (phase > 0x100) {
            vphase[type].repeat_skip++;
        }

        vphase[type].phase = phase & 0xff;

        if (vphase[type].repeat_skip > 5) {
            vphase[type].repeat_skip = 5;
        }
    }
}

static void
vpp_set_filters2(u32 width_in,
                 u32 height_in,
                 u32 width_out,
                 u32 height_out,
                 u32 aspect_ratio_out,
                 u32 vpp_flags,
                 vpp_frame_par_t *next_frame_par)
{
    u32 screen_width, screen_height;
    s32 start, end;
    s32 video_top, video_left, temp;
    u32 video_width, video_height;
    u32 ratio_x = 0;
    u32 ratio_y = 0;
    vppfilter_mode_t *filter = &next_frame_par->vpp_filter;
    u32 wide_mode = vpp_flags & VPP_FLAG_WIDEMODE_MASK;
    s32 height_shift = 0;
    u32 height_after_ratio;
    u32 aspect_factor;
    s32 ini_vphase;

    next_frame_par->VPP_prog_as_interlace = 0;

RESTART:

    if (vpp_flags & VPP_FLAG_INTERLACE_OUT) {
        height_shift++;
    }

    if (vpp_flags & VPP_FLAG_INTERLACE_IN) {
        height_shift--;
    }

    aspect_factor = (vpp_flags & VPP_FLAG_AR_MASK) >> VPP_FLAG_AR_BITS;

    /* keep 8 bits resolution for aspect conversion */
    if ((aspect_factor == 0) || (wide_mode == VIDEO_WIDEOPTION_WIDE)) {
        aspect_factor = 0x100;

    } else {
        aspect_factor = (width_in * height_out * aspect_factor << 3) /
            ((width_out * height_in * aspect_ratio_out) >> 5);
    }
    height_after_ratio = (height_in * aspect_factor) >> 8;

    /* if we have ever set a cropped display area for video layer
     * (by checking video_layer_width/video_height), then
     * it will override the input width_out/height_out for
     * ratio calculations, a.k.a we have a window for video content
     */

    if ((video_layer_width == 0) || (video_layer_height == 0)) {
        video_top = 0;
        video_left = 0;
        video_width = width_out;
        video_height = height_out;

    } else {
        video_top = video_layer_top;
        video_left = video_layer_left;
        video_width = video_layer_width;
        video_height = video_layer_height;
    }

    screen_width = video_width;
    screen_height = video_height;

    ratio_x = (width_in << 18) / screen_width;
    if (ratio_x * screen_width < (width_in << 18)) {
        ratio_x++;
    }

    ratio_y = (height_after_ratio << 18) / screen_height;

    if (wide_mode == VIDEO_WIDEOPTION_NORMAL) {
        ratio_x = ratio_y = max(ratio_x, ratio_y);
        ratio_y = (ratio_y << 8) / aspect_factor;
    }

    /* vertical */
    ini_vphase = vpp_zoom_center_y & 0xff;

    if (vpp_flags & VPP_FLAG_INTERLACE_IN) {
        next_frame_par->VPP_pic_in_height_ = height_in >> 1;

    } else {
        next_frame_par->VPP_pic_in_height_ = height_in;
    }

    /* screen position for source */
    start = video_top + video_height / 2 - ((height_in << 17) + (vpp_zoom_center_y << 10)) / ratio_y;
    end   = (height_in << 18) / ratio_y + start - 1;
   
    /* calculate source vertical clip */
    if (video_top < 0) {
        if (start < 0) {
            temp = (-start * ratio_y) >> 18;
            next_frame_par->VPP_vd_start_lines_ = temp;

        } else {
            next_frame_par->VPP_vd_start_lines_ = 0;
        }

    } else {
        if (start < video_top) {
            temp = ((video_top - start) * ratio_y) >> 18;
            next_frame_par->VPP_vd_start_lines_ = temp;

        } else {
            next_frame_par->VPP_vd_start_lines_ = 0;
        }
    }
    temp = next_frame_par->VPP_vd_start_lines_ + (video_height * ratio_y >> 18);
    next_frame_par->VPP_vd_end_lines_ = (temp <= (height_in - 1))? temp :(height_in -1);
    /* find overlapped region between
     * [start, end], [0, height_out-1], [video_top, video_top+video_height-1]
     */
    start = max(start, max(0, video_top));
    end   = min(end, min((s32)height_out-1, (s32)(video_top+video_height-1)));

    if (start >= end) {
        /* nothing to display */
        next_frame_par->VPP_vsc_startp = 0;

        next_frame_par->VPP_vsc_endp = 0;

    } else {
        next_frame_par->VPP_vsc_startp =
            (vpp_flags & VPP_FLAG_INTERLACE_OUT) ? (start >> 1) : start;

        next_frame_par->VPP_vsc_endp =
            (vpp_flags & VPP_FLAG_INTERLACE_OUT) ? (end >> 1) : end;
    }

    /* check the painful bandwidth limitation and see
     * if we need skip half resolution on source side for progressive
     * frames.
     */
    if ((next_frame_par->VPP_prog_as_interlace == 0) &&
        (height_in >= 1080) &&
        (ratio_y > 0x81000) &&
        ((vpp_flags & VPP_FLAG_INTERLACE_IN) == 0)) {
        /* ratio_y > 2.25 */
        vpp_flags |= VPP_FLAG_INTERLACE_IN;
        next_frame_par->VPP_prog_as_interlace = 1;
        goto RESTART;
    }

    /* set filter co-efficients */
    /* adjust ratio_y for those interlacing stuff */
    if (height_shift >= 0) {
        ratio_y <<= height_shift;

    } else {
        ratio_y >>= -height_shift;
    }

    if (vpp_flags & VPP_FLAG_INTERLACE_OUT)
        filter->vpp_vert_coeff = filter_table[COEF_BILINEAR];
    else
        filter->vpp_vert_coeff = filter_table[COEF_BICUBIC];

    filter->vpp_vsc_start_phase_step = ratio_y << 6;

    f2v_get_vertical_phase(ratio_y, ini_vphase,
                           next_frame_par->VPP_vf_ini_phase_,
                           vpp_flags & VPP_FLAG_INTERLACE_OUT);

    /* horizontal */

    /* set register to hardware reset default values when VPP scaler is working under
     * normal linear mode
     * VIDEO_WIDEOPTION_CINEMAWIDE case register value is set inside
     * calculate_non_linear_ratio()
     */
    filter->vpp_hf_start_phase_slope = 0;
    filter->vpp_hf_end_phase_slope   = 0;
    filter->vpp_hf_start_phase_step  = ratio_x << 6;

    next_frame_par->VPP_hsc_linear_startp = next_frame_par->VPP_hsc_startp;
    next_frame_par->VPP_hsc_linear_endp   = next_frame_par->VPP_hsc_endp;

    filter->vpp_horz_coeff = filter_table[COEF_BICUBIC];

    filter->vpp_hsc_start_phase_step = ratio_x << 6;
    next_frame_par->VPP_hf_ini_phase_ = vpp_zoom_center_x & 0xff;

    if ((ratio_x == (1 << 18)) && (next_frame_par->VPP_hf_ini_phase_ == 0))
        filter->vpp_horz_coeff = vpp_filter_coefs_bicubic_sharp;
    else
        filter->vpp_horz_coeff = filter_table[COEF_BICUBIC];

    /* screen position for source */
    start = video_left + video_width / 2 - ((width_in << 17) + (vpp_zoom_center_x << 10)) / ratio_x;
    end   = (width_in << 18) / ratio_x + start - 1;
    /* calculate source horizontal clip */
    if (video_left < 0) {
        if (start < 0) {
            temp = (-start * ratio_x) >> 18;
            next_frame_par->VPP_hd_start_lines_ = temp;

        } else {
            next_frame_par->VPP_hd_start_lines_ = 0;
        }

    } else {
        if (start < video_left) {
            temp = ((video_left - start) * ratio_x) >> 18;
            next_frame_par->VPP_hd_start_lines_ = temp;

        } else {
            next_frame_par->VPP_hd_start_lines_ = 0;
        }
    }

    temp = next_frame_par->VPP_hd_start_lines_ + (video_width * ratio_x >> 18);
    next_frame_par->VPP_hd_end_lines_ = (temp <= (width_in - 1))? temp :(width_in -1);
    next_frame_par->VPP_line_in_length_ = next_frame_par->VPP_hd_end_lines_ - next_frame_par->VPP_hd_start_lines_ + 1;
    /* find overlapped region between
     * [start, end], [0, width_out-1], [video_left, video_left+video_width-1]
     */
    start = max(start, max(0, video_left));
    end   = min(end, min((s32)width_out-1, (s32)(video_left+video_width-1)));

    if (start >= end) {
        /* nothing to display */
        next_frame_par->VPP_hsc_startp = 0;

        next_frame_par->VPP_hsc_endp = 0;

    } else {
        next_frame_par->VPP_hsc_startp = start;

        next_frame_par->VPP_hsc_endp = end;
    }

    filter->vpp_hsc_start_phase_step = ratio_x << 6;

    next_frame_par->VPP_hf_ini_phase_ = vpp_zoom_center_x & 0xff;
}

void
vpp_set_filters(u32 wide_mode,
                vframe_t *vf,
                vpp_frame_par_t *next_frame_par,
                const vinfo_t *vinfo)
{
    u32 src_width = 0;
    u32 src_height = 0;
    u32 vpp_flags = 0;
    u32 aspect_ratio = 0;

	BUG_ON(vinfo == NULL);

    next_frame_par->VPP_post_blend_vd_v_start_ = 0;
    next_frame_par->VPP_post_blend_vd_h_start_ = 0;

    next_frame_par->VPP_postproc_misc_ = 0x200;

    /* check force ratio change flag in display buffer also
     * if it exist then it will override the settings in display side
     */
    if (vf->ratio_control & DISP_RATIO_FORCECONFIG) {
        if ((vf->ratio_control & DISP_RATIO_CTRL_MASK) == DISP_RATIO_KEEPRATIO) {
            if (wide_mode == VIDEO_WIDEOPTION_WIDE) {
                wide_mode = VIDEO_WIDEOPTION_NORMAL;
            }

        } else {
            if (wide_mode == VIDEO_WIDEOPTION_NORMAL) {
                wide_mode = VIDEO_WIDEOPTION_WIDE;
            }
        }
    }

    aspect_ratio = (vf->ratio_control & DISP_RATIO_ASPECT_RATIO_MASK)
                     >> DISP_RATIO_ASPECT_RATIO_BIT;

    if (vf->type & VIDTYPE_INTERLACE) {
        vpp_flags = VPP_FLAG_INTERLACE_IN;
    }

    src_width = vf->width;
    src_height = vf->height;

    vpp_wide_mode = wide_mode;
    vpp_flags |= wide_mode | (aspect_ratio << VPP_FLAG_AR_BITS);
    
    if (vinfo->field_height != vinfo->height)
    	vpp_flags |= VPP_FLAG_INTERLACE_OUT;

    next_frame_par->VPP_post_blend_vd_v_end_ = vinfo->field_height - 1;
    next_frame_par->VPP_post_blend_vd_h_end_ = vinfo->width - 1;
    next_frame_par->VPP_post_blend_h_size_ = vinfo->width;

    vpp_set_filters2(src_width,
                     src_height,
                     vinfo->width,
                     vinfo->height,
                     (vinfo->aspect_ratio_den << 8) / vinfo->aspect_ratio_num,
                     vpp_flags,
                     next_frame_par);
}

void vpp_set_video_layer_position(s32 x0, s32 y0, u32 x1, u32 y1)
{
    video_layer_top =y0;
    video_layer_left = x0;
    video_layer_width = x1-x0;
    video_layer_height = y1-y0;
}

void vpp_get_video_layer_position(s32* top,
                                  s32* left,
                                  u32* width,
                                  u32* height)
{
    *top = video_layer_top;
    *left = video_layer_left;
    *width = video_layer_width;
    *height = video_layer_height;
}
