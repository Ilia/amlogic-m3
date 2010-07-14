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
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/amports/ptsserv.h>
#include <linux/amports/amstream.h>
#include <linux/device.h>

#include <asm/uaccess.h>
#include <mach/am_regs.h>

#include "streambuf_reg.h"
#include "streambuf.h"

#include "tsdemux.h"

const static char tsdemux_fetch_id[] = "tsdemux-fetch-id";
const static char tsdemux_irq_id[] = "tsdemux-irq-id";

static DECLARE_WAIT_QUEUE_HEAD(wq);
static u32 fetch_done;
static u32 discontinued_counter;

static irqreturn_t tsdemux_isr(int irq, void *dev_id)
{
    u32 int_status = READ_MPEG_REG(STB_INT_STATUS);

    if (int_status & (1 << NEW_PDTS_READY)) {
        u32 pdts_status = READ_MPEG_REG(STB_PTS_DTS_STATUS);
        if (pdts_status & (1 << VIDEO_PTS_READY))
            pts_checkin_wrptr(PTS_TYPE_VIDEO, 
                READ_MPEG_REG(VIDEO_PDTS_WR_PTR), READ_MPEG_REG(VIDEO_PTS_DEMUX));

        if (pdts_status & (1 << AUDIO_PTS_READY))
            pts_checkin_wrptr(PTS_TYPE_AUDIO, 
                READ_MPEG_REG(AUDIO_PDTS_WR_PTR), READ_MPEG_REG(AUDIO_PTS_DEMUX));

        WRITE_MPEG_REG(STB_PTS_DTS_STATUS, pdts_status);
    }
   if(int_status & (1<<DIS_CONTINUITY_PACKET) ){
	discontinued_counter++; 
	//printk("discontinued counter=%d\n",discontinued_counter);
   }
   if (int_status & (1<<SUB_PES_READY))
    {
        /* TODO: put data to somewhere */
        printk("subtitle pes ready\n");
    }
   
    WRITE_MPEG_REG(STB_INT_STATUS, int_status);

    return IRQ_HANDLED;
}

static irqreturn_t parser_isr(int irq, void *dev_id)
{
    u32 int_status = READ_MPEG_REG(PARSER_INT_STATUS);

    WRITE_MPEG_REG(PARSER_INT_STATUS, int_status);

    if (int_status & PARSER_INTSTAT_FETCH_CMD) {
        fetch_done = 1;

        wake_up_interruptible(&wq);
    }


    return IRQ_HANDLED;
}

static ssize_t _tsdemux_write(const char __user *buf, size_t count)
{
    size_t r = count;
    const char __user *p = buf;
    u32 len;

    if (r > 0) {
        len = min(r, (size_t)FETCHBUF_SIZE);

        copy_from_user(fetchbuf_remap, p, len);

        fetch_done = 0;

        wmb();

        WRITE_MPEG_REG(PARSER_FETCH_ADDR, fetchbuf);
        WRITE_MPEG_REG(PARSER_FETCH_CMD, (7 << FETCH_ENDIAN) | len);

        if (wait_event_interruptible(wq, fetch_done != 0))
            return -ERESTARTSYS;

        p += len;
        r -= len;
    }

    return count -r;
}


s32 tsdemux_init(u32 vid, u32 aid, u32 sid)
{
    s32 r;
    u32 parser_sub_start_ptr;
    u32 parser_sub_end_ptr;
    u32 parser_sub_rp;

    parser_sub_start_ptr = READ_MPEG_REG(PARSER_SUB_START_PTR);
    parser_sub_end_ptr = READ_MPEG_REG(PARSER_SUB_END_PTR);
    parser_sub_rp = READ_MPEG_REG(PARSER_SUB_RP);

    WRITE_MPEG_REG(RESET1_REGISTER, RESET_PARSER | RESET_DEMUXSTB);

    WRITE_MPEG_REG(STB_TOP_CONFIG, 0);
    WRITE_MPEG_REG(DEMUX_CONTROL, 0);

    /* set PID filter */
    printk("tsdemux video_pid = 0x%x, audio_pid = 0x%x, sub_pid = 0x%x\n",
           vid, aid, sid);
    WRITE_MPEG_REG(FM_WR_DATA,
                   (((vid & 0x1fff) | (VIDEO_PACKET << 13)) << 16) |
                    ((aid & 0x1fff) | (AUDIO_PACKET << 13)));
    WRITE_MPEG_REG(FM_WR_ADDR, 0x8000);
    while (READ_MPEG_REG(FM_WR_ADDR) & 0x8000);

    WRITE_MPEG_REG(FM_WR_DATA, 
                   (((sid & 0x1fff) | (SUB_PACKET << 13)) << 16) | 0xffff);
    WRITE_MPEG_REG(FM_WR_ADDR, 0x8001);
    while (READ_MPEG_REG(FM_WR_ADDR) & 0x8000);

    WRITE_MPEG_REG(MAX_FM_COMP_ADDR, 1);

    WRITE_MPEG_REG(STB_INT_MASK, 0);
    WRITE_MPEG_REG(STB_INT_STATUS, 0xffff);

    /* TS data path */
    WRITE_MPEG_REG(FEC_INPUT_CONTROL, 0x7000);
    WRITE_MPEG_REG(DEMUX_MEM_REQ_EN,
                   (1 << VIDEO_PACKET) |
                   (1 << AUDIO_PACKET) |
                   (1 << SUB_PACKET));
    WRITE_MPEG_REG(DEMUX_ENDIAN,
                   (7 << OTHER_ENDIAN)  |
                   (7 << BYPASS_ENDIAN) |
                   (0 << SECTION_ENDIAN));
    WRITE_MPEG_REG(TS_HIU_CTL, 1 << USE_HI_BSF_INTERFACE);
    WRITE_MPEG_REG(TS_FILE_CONFIG,
                   (6 << DES_OUT_DLY)                      |
                   (3 << TRANSPORT_SCRAMBLING_CONTROL_ODD) |
                   (1 << TS_HIU_ENABLE)                    |
                   (4 << FEC_FILE_CLK_DIV));

    /* enable TS demux */
    WRITE_MPEG_REG(DEMUX_CONTROL, (1 << STB_DEMUX_ENABLE));

    if (fetchbuf == 0)
    {
        printk("%s: no fetchbuf\n", __FUNCTION__);
        return -ENOMEM;
    }

    /* hook stream buffer with PARSER */
    WRITE_MPEG_REG(PARSER_VIDEO_START_PTR,
                   READ_MPEG_REG(VLD_MEM_VIFIFO_START_PTR));
    WRITE_MPEG_REG(PARSER_VIDEO_END_PTR,
                   READ_MPEG_REG(VLD_MEM_VIFIFO_END_PTR));
    CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_VID_MAN_RD_PTR);

    WRITE_MPEG_REG(PARSER_AUDIO_START_PTR,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));
    WRITE_MPEG_REG(PARSER_AUDIO_END_PTR,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_END_PTR));
    CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_AUD_MAN_RD_PTR);

    WRITE_MPEG_REG(PARSER_CONFIG,
                   (10 << PS_CFG_PFIFO_EMPTY_CNT_BIT) |
                   (1  << PS_CFG_MAX_ES_WR_CYCLE_BIT) |
                   (16 << PS_CFG_MAX_FETCH_CYCLE_BIT));

    WRITE_MPEG_REG(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
    CLEAR_MPEG_REG_MASK(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

    WRITE_MPEG_REG(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
    CLEAR_MPEG_REG_MASK(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

    WRITE_MPEG_REG(PARSER_SUB_START_PTR, parser_sub_start_ptr);
    WRITE_MPEG_REG(PARSER_SUB_END_PTR, parser_sub_end_ptr);
    WRITE_MPEG_REG(PARSER_SUB_RP, parser_sub_rp);
    SET_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_SUB_MAN_RD_PTR);

    if ((r = pts_start(PTS_TYPE_VIDEO)) < 0)
    {
        printk("Video pts start  failed.(%d)\n",r);
        goto err1;
    }

    if ((r = pts_start(PTS_TYPE_AUDIO)) < 0)
    {
        printk("Audio pts start failed.(%d)\n",r);
        goto err2;
    }

    r = request_irq(INT_PARSER, parser_isr,
                    IRQF_SHARED, "tsdemux-fetch",
                    (void *)tsdemux_fetch_id);
    if (r) 
        goto err3;
    WRITE_MPEG_REG(PARSER_INT_STATUS, 0xffff);
    WRITE_MPEG_REG(PARSER_INT_ENABLE, PARSER_INTSTAT_FETCH_CMD<<8);

    discontinued_counter=0;
    r = request_irq(INT_DEMUX, tsdemux_isr,
                    IRQF_SHARED, "tsdemux-irq",
                    (void *)tsdemux_irq_id);
    WRITE_MPEG_REG(STB_INT_MASK, 
                    (1 << SUB_PES_READY) 
                  | (1 << NEW_PDTS_READY)
                  | (1 << DIS_CONTINUITY_PACKET));
    if (r) 
        goto err4;

    return 0;

err4:
    free_irq(INT_PARSER, (void *)tsdemux_fetch_id);
err3:
    pts_stop(PTS_TYPE_AUDIO);
err2:
    pts_stop(PTS_TYPE_VIDEO);
err1:
    printk("TS Demux init failed.\n");
    return -ENOENT;
}

void tsdemux_release(void)
{
    WRITE_MPEG_REG(PARSER_INT_ENABLE, 0);

    free_irq(INT_PARSER, (void *)tsdemux_fetch_id);

    WRITE_MPEG_REG(STB_INT_MASK, 0);

    free_irq(INT_DEMUX, (void *)tsdemux_irq_id);

    pts_stop(PTS_TYPE_VIDEO);
    pts_stop(PTS_TYPE_AUDIO);
}

ssize_t tsdemux_write(struct file *file,
                      struct stream_buf_s *vbuf,
                      struct stream_buf_s *abuf,
                      const char __user *buf, size_t count)
{
    s32 r;
    stream_port_t *port = (stream_port_t *)file->private_data;
    size_t wait_size,write_size;
   
    if ((stbuf_space(vbuf) < count) ||
        (stbuf_space(abuf) < count)) {
        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;

        wait_size=min(stbuf_size(vbuf)/8,stbuf_size(abuf)/4);
        if ((port->flag & PORT_FLAG_VID) 
           && (stbuf_space(vbuf) < wait_size)) {
            r = stbuf_wait_space(vbuf, wait_size);

            if (r < 0)
                return r;
        }

        if ((port->flag & PORT_FLAG_AID)
           && (stbuf_space(abuf) < wait_size)) {
            r = stbuf_wait_space(abuf, wait_size);

            if (r < 0)
                return r;
        }
    }
    write_size=min(stbuf_space(vbuf),stbuf_space(abuf));
    write_size=min(count,write_size);
    if(write_size>0)
    	return _tsdemux_write(buf, write_size);
    else
	return -EAGAIN;
}

static ssize_t show_discontinue_counter(struct class *class, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", discontinued_counter);
}

static struct class_attribute tsdemux_class_attrs[] = {
    __ATTR(discontinue_counter,  S_IRUGO, show_discontinue_counter,    NULL  ),
    __ATTR_NULL
};

static struct class tsdemux_class = {
    .name = "tsdemux",
    .class_attrs = tsdemux_class_attrs,
};


int     tsdemux_class_register(void)
{
  	int r;
	if((r=class_register(&tsdemux_class))<0)
		{
		printk("register tsdemux class error!\n");		
		}
	discontinued_counter=0;
	return r;
}
void  tsdemux_class_unregister(void)
{
 	class_unregister(&tsdemux_class);
}

void tsdemux_change_avid(unsigned int vid, unsigned int aid)
{
    WRITE_MPEG_REG(FM_WR_DATA,
               (((vid & 0x1fff) | (VIDEO_PACKET << 13)) << 16) |
                ((aid & 0x1fff) | (AUDIO_PACKET << 13)));
    WRITE_MPEG_REG(FM_WR_ADDR, 0x8000);
    while (READ_MPEG_REG(FM_WR_ADDR) & 0x8000);

    return;
}

void tsdemux_audio_reset(void)
{
    ulong flags;
    spinlock_t lock = SPIN_LOCK_UNLOCKED;

    spin_lock_irqsave(&lock, flags);

    WRITE_MPEG_REG(PARSER_AUDIO_WP,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));
    WRITE_MPEG_REG(PARSER_AUDIO_RP,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));
    
    WRITE_MPEG_REG(PARSER_AUDIO_START_PTR,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));
    WRITE_MPEG_REG(PARSER_AUDIO_END_PTR,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_END_PTR));
    CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_AUD_MAN_RD_PTR);

    WRITE_MPEG_REG(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
    CLEAR_MPEG_REG_MASK(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

    spin_unlock_irqrestore(&lock, flags);

    return;
}
