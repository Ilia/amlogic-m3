#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/cache.h>
#include <asm/cacheflush.h>
#include <asm/arch/am_regs.h>
#include <asm/bsp.h>

#include "dsp_microcode.h"
#include "audiodsp_module.h"
#include "dsp_control.h"
#include <asm/dsp/dsp_register.h>

#include "dsp_mailbox.h"
#include <linux/delay.h>

#define MIN_CACHE_ALIGN(x)	(((x-4)&(~0x1f)))
#define MAX_CACHE_ALIGN(x)	((x+0x1f)&(~0x1f))


static void	enable_dsp(int flag)
{	
	 int reg_save = READ_ISA_REG(IREG_DDR_CTRL);

#if defined(AML_APOLLO) || defined(AML_A1H)
		/* RESET DSP */
	 if(!flag)
	  	 CLEAR_ISA_REG_MASK(IREG_ARC2_CTRL, 1);
	/*write more for make the dsp is realy reset!*/
	 SET_MPEG_REG_MASK(RESET1_REGISTER, RESET_ARC2);
	 SET_MPEG_REG_MASK(RESET1_REGISTER, RESET_ARC2);
	 SET_MPEG_REG_MASK(RESET1_REGISTER, RESET_ARC2);
	 
#if defined(AML_A1H)
    	/* Enable BVCI low 16MB address mapping to DDR */
	
    	SET_ISA_REG_MASK(IREG_DDR_CTRL, (1<<DDR_CTL_MAPDDR));
    	/* polling highest bit of IREG_DDR_CTRL until the mapping is done */
    	while (READ_ISA_REG(IREG_DDR_CTRL) & (1<<31));
#endif
	
        if (flag) {
		    SET_ISA_REG_MASK(IREG_ARC2_CTRL, 1);
		    CLEAR_ISA_REG_MASK(IREG_ARC2_CTRL, 1);
		}
	/* DSP is running, restore the original view of address 0 (if necessary) */
	WRITE_ISA_REG(IREG_DDR_CTRL, reg_save);	
#endif		
}

void halt_dsp( struct audiodsp_priv *priv)
{
	int i=0;
	if(DSP_RD(DSP_STATUS)==DSP_STATUS_RUNING)
		{
		dsp_mailbox_send(priv,1,M2B_IRQ0_DSP_HALT,0,0,0);
		msleep(1);/*waiting cpu to self halted*/
		}
	if(DSP_RD(DSP_STATUS)!=DSP_STATUS_RUNING)
		{
		DSP_WD(DSP_STATUS, DSP_STATUS_HALT);
		return ;
		}
	enable_dsp(0);/*hardware halt the cpu*/
	DSP_WD(DSP_STATUS, DSP_STATUS_HALT);
	priv->last_stream_fmt=-1;/*mask the stream format is not valid*/
}
void reset_dsp( struct audiodsp_priv *priv)
{
    halt_dsp(priv);

    flush_and_inv_dcache_all();
    /* map DSP 0 address so that reset vector points to same vector table as ARC1 */
    CLEAR_ISA_REG_MASK(IREG_ARC2_CTRL, (0xfff << 4));
    SET_ISA_REG_MASK(IREG_ARC2_CTRL, ((0x80000000)>> 20) << 4);
    enable_dsp(1);

    return;    
}
static inline int dsp_set_stack( struct audiodsp_priv *priv)
{
	if(priv->dsp_stack_start==0)
		priv->dsp_stack_start=(unsigned long)kmalloc(priv->dsp_stack_size,GFP_KERNEL);
	if(priv->dsp_stack_start==0)
		{
		DSP_PRNT("kmalloc error,no memory for audio dsp stack\n");
		return -ENOMEM;
		}
	memset((void*)priv->dsp_stack_start,0,priv->dsp_stack_size);
	DSP_WD(DSP_STACK_START,MAX_CACHE_ALIGN(priv->dsp_stack_start));
	DSP_WD(DSP_STACK_END,MIN_CACHE_ALIGN(priv->dsp_stack_start+priv->dsp_stack_size));
	//DSP_PRNT("DSP statck start =%#lx,size=%#lx\n",priv->dsp_stack_start,priv->dsp_stack_size);
	if(priv->dsp_gstack_start==0)
		priv->dsp_gstack_start=(unsigned long)kmalloc(priv->dsp_gstack_size,GFP_KERNEL);
	if(priv->dsp_gstack_start==0)
		{
		DSP_PRNT("kmalloc error,no memory for audio dsp gp stack\n");
		kfree((void *)priv->dsp_stack_start);
		return -ENOMEM;
		}
	memset((void*)priv->dsp_gstack_start,0,priv->dsp_gstack_size);
	DSP_WD(DSP_GP_STACK_START,MAX_CACHE_ALIGN(priv->dsp_gstack_start));
	DSP_WD(DSP_GP_STACK_END,MIN_CACHE_ALIGN(priv->dsp_gstack_start+priv->dsp_gstack_size));
	//DSP_PRNT("DSP gp statck start =%#lx,size=%#lx\n",priv->dsp_gstack_start,priv->dsp_gstack_size);
		
	return 0;
}
static inline int dsp_set_heap( struct audiodsp_priv *priv)
{
	if(priv->dsp_heap_size==0)
		return 0;
	if(priv->dsp_heap_start==0)
		priv->dsp_heap_start=(unsigned long)kmalloc(priv->dsp_heap_size,GFP_KERNEL);
	if(priv->dsp_heap_start==0)
		{
		DSP_PRNT("kmalloc error,no memory for audio dsp dsp_set_heap\n");
		return -ENOMEM;
		}
	memset((void *)priv->dsp_heap_start,0,priv->dsp_heap_start);
	DSP_WD(DSP_MEM_START,MAX_CACHE_ALIGN(priv->dsp_heap_start));
	DSP_WD(DSP_MEM_END,MIN_CACHE_ALIGN(priv->dsp_heap_start+priv->dsp_heap_size));
	//DSP_PRNT("DSP heap start =%#lx,size=%#lx\n",priv->dsp_heap_start,priv->dsp_heap_size);
	return 0;
}

static inline int dsp_set_stream_buffer( struct audiodsp_priv *priv)
{
	if(priv->stream_buffer_mem_size==0)
		{
		DSP_WD(DSP_DECODE_OUT_START_ADDR,0);
		DSP_WD(DSP_DECODE_OUT_END_ADDR,0);
		DSP_WD(DSP_DECODE_OUT_RD_ADDR,0);
		DSP_WD(DSP_DECODE_OUT_WD_ADDR,0);
		return 0;
		}
	if(priv->stream_buffer_mem==NULL)
		priv->stream_buffer_mem=(void*)kmalloc(priv->stream_buffer_mem_size,GFP_KERNEL);
	if(priv->stream_buffer_mem==NULL)
		{
		DSP_PRNT("kmalloc error,no memory for audio dsp stream buffer\n");
		return -ENOMEM;
		}
	memset((void *)priv->stream_buffer_mem,0,priv->stream_buffer_mem_size);
	priv->stream_buffer_start=MAX_CACHE_ALIGN((unsigned long)priv->stream_buffer_mem);
	priv->stream_buffer_end=MIN_CACHE_ALIGN((unsigned long)priv->stream_buffer_mem+priv->stream_buffer_mem_size);
	priv->stream_buffer_size=priv->stream_buffer_end-priv->stream_buffer_start;
	if(priv->stream_buffer_size<0)
		{
		DSP_PRNT("Stream buffer set error,must more larger,mensize=%d,buffer size=%ld\n",
			priv->stream_buffer_mem_size,priv->stream_buffer_size
			);
		kfree(priv->stream_buffer_mem);
		priv->stream_buffer_mem=NULL;
		return -2;
		}
		
	DSP_WD(DSP_DECODE_OUT_START_ADDR,priv->stream_buffer_start);
	DSP_WD(DSP_DECODE_OUT_END_ADDR,priv->stream_buffer_end);
	DSP_WD(DSP_DECODE_OUT_RD_ADDR,priv->stream_buffer_start);
	DSP_WD(DSP_DECODE_OUT_WD_ADDR,priv->stream_buffer_start);
	
	DSP_PRNT("DSP stream buffer to [%#lx-%#lx]\n",priv->stream_buffer_start,priv->stream_buffer_end);
	return 0;
}


 int dsp_start( struct audiodsp_priv *priv, struct auidodsp_microcode *mcode)
 {
	int i;
	int res;
	mutex_lock(&priv->dsp_mutex);		
	halt_dsp(priv);
	if(priv->stream_fmt!=priv->last_stream_fmt)
		{
		if(auidodsp_microcode_load(audiodsp_privdata(),mcode)!=0)
			{
			printk("load microcode error\n");
			res=-1;
			goto exit;
			}
		priv->last_stream_fmt=priv->stream_fmt;
		}
	if((res=dsp_set_stack(priv)))
		goto exit;
	if((res=dsp_set_heap(priv)))
		goto exit;
	if((res=dsp_set_stream_buffer(priv)))
		goto exit;
	reset_dsp(priv);
	priv->dsp_start_time=jiffies;
	for(i=0;i<1000;i++)
		{
		if(DSP_RD(DSP_STATUS)==DSP_STATUS_RUNING)
			break;
		msleep(1);
		}
	if(i>=1000)
		{
		DSP_PRNT("dsp not running \n");
		res=-1;
		}
	else
		{
		DSP_PRNT("dsp status=%lx\n",DSP_RD(DSP_STATUS));
		priv->dsp_is_started=1;
		res=0;
		}
exit:
	mutex_unlock(&priv->dsp_mutex);		
	return res;
 }

 int dsp_stop( struct audiodsp_priv *priv)
 	{
 	mutex_lock(&priv->dsp_mutex);		
 	priv->dsp_is_started=0;
 	halt_dsp(priv);
	priv->dsp_end_time=jiffies;
	if(priv->dsp_stack_start!=0)
		kfree((void*)priv->dsp_stack_start);
	priv->dsp_stack_start=0;
	if(priv->dsp_heap_start!=0)
		kfree((void*)priv->dsp_heap_start);
	priv->dsp_heap_start=0;
	if(priv->stream_buffer_mem!=NULL)
		{
		kfree(priv->stream_buffer_mem);
		priv->stream_buffer_mem=NULL;		
		}
	mutex_unlock(&priv->dsp_mutex);	
	return 0;
 	}

