/*
 * ECC kernel module (C) 1998, 1999, 2002 Dan Hollis <goemon at anime dot net>
 * Portions thanks to
 *	Michael O'Reilly <michael at metal dot iinet dot net dot au>
 *	Osma Ahvenlampi <oa at spray dot fi>
 *	Martin Maney <maney at pobox dot com>
 *	Peter Heckert <peter.heckert at arcormail dot de>
 *      Ishikawa <ishikawa at yk dot rim dot or dot jp>
 *	Jaakko Hyvti <jaakko dot hyvatti at iki dot fi>
 *      Various Linux Networx, LLNL & Red Hat staff.
 */

#define DEBUG	0

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/timer.h>
#include <asm/io.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>

#define	ECC_VER	"0.14 (Oct 10 2001)"

static struct timer_list ecctimer;
static struct pci_dev *bridge = NULL;
static u16 vendor, device;
static int scrub_needed;
static int scrub_row;

/* These are tunable by either module parameter or sysctl */
int panic_on_mbe = 1;
int log_mbe = 1;
int log_sbe = 1;
int poll_msec = 1000;

#ifdef MODULE
MODULE_PARM(panic_on_mbe, "i");
MODULE_PARM(log_mbe, "i");
MODULE_PARM(log_sbe, "i");
MODULE_PARM(poll_msec, "i");
#endif

#define CTL_ECC 2740

static ctl_table ecc_table[] = {
	{1, "panic_on_mbe",
		&panic_on_mbe, sizeof(int), 0644, NULL, &proc_dointvec},
	{2, "log_mbe",
		&log_mbe, sizeof(int), 0644, NULL, &proc_dointvec},
	{3, "log_sbe",
		&log_sbe, sizeof(int), 0644, NULL, &proc_dointvec},
	{4, "poll_msec",
		&poll_msec, sizeof(int), 0644, NULL, &proc_dointvec},
	{0}
};

static ctl_table ecc_root_table[] = {
	{CTL_DEBUG, "ecc", NULL, 0, 0555, ecc_table},
	{0}
};

static struct ctl_table_header *ecc_sysctl_header = NULL;

#define MAXBANKS 16

#define I860_DUMPCONFIG 0

#define mbe_printk(fmt,args...) do { \
    if (panic_on_mbe) panic("ECC: MBE " fmt,##args); \
    else if (log_mbe) printk(KERN_EMERG "ECC: MBE " fmt,##args); \
} while(0)
#define sbe_printk(fmt,args...) do { \
    if (log_sbe) printk(KERN_WARNING "ECC: SBE " fmt,##args); \
} while(0)

#if     defined(NDEBUG)
#define assert(EX) do { } while (0)
#else
#define assert(EX) do { \
	if (!(EX)) printk("assertion failed: %s, %s::%d\n", \
			#EX, __FILE__, __LINE__); \
	} while (0)
#endif

/* memory types */
#define BANK_EMPTY	0	/* Empty bank */
#define BANK_RESERVED	1	/* Reserved bank type */
#define BANK_UNKNOWN	2	/* Unknown bank type */
#define BANK_FPM	3	/* Fast page mode */
#define BANK_EDO	4	/* Extended data out */
#define BANK_BEDO	5	/* Burst Extended data out */
#define BANK_SDR	6	/* Single data rate SDRAM */
#define BANK_DDR	7	/* Double data rate SDRAM */
#define BANK_RDR	8	/* Registered SDRAM */
#define BANK_RMBS	9	/* Rambus DRAM */

/* Memory bank info */
static struct bankstruct
{
	u32 endaddr;		/* bank ending address */
	u32 mbecount;		/* total number of MBE errors */
	u32 sbecount;		/* total number of SBE errors */
	u8 eccmode;		/* ECC enabled for this bank? */
	u8 mtype;		/* memory bank type */
} bank[MAXBANKS];			/* do any chipsets support more? */

/* chipset ECC capabilities and mode */
#define ECC_NONE	0	/* Doesnt support ECC (or is BIOS disabled) */
#define ECC_RESERVED	1	/* Reserved ECC type */
#define ECC_PARITY	2	/* Detects parity errors */
#define ECC_DETECT	3	/* Detects ECC errors */
#define ECC_CORRECT	4	/* Detects ECC errors and corrects SBE */
#define ECC_AUTO	5	/* Detects ECC errors and has hardware scrubber */

static struct ChipsetInfo
{
	int ecc_cap;		/* chipset ECC capabilities */
	int ecc_mode;		/* current ECC mode */
	void (*check)(void);	/* pointer to ecc checking routine */
	void (*clear_err)(void);/* pointer to error clear routine */
#if 0
/*
 * I dont think we care about SERR at the moment.
 * We may if/when we hook into an NMI handler.
 */
	int SERR;		/* SERR enabled? */
	int SERR_MBE;		/* SERR on multi-bit error? */
	int SERR_SBE;		/* SERR on single-bit error? */
#endif
	int MBE_flag_address;	/* pci offset for mbe register */
	int MBE_flag_shift;	/* bits to shift for mbe flag */
	int MBE_flag_mask;	/* mask for mbe flag */
	int MBE_row_shift;	/* bits to shift for mbe row flag */
	int MBE_row_mask;	/* mask for mbe register (shifted) */
	int SBE_flag_address;	/* pci offset for sbe register */
	int SBE_flag_shift;	/* bits to shift for sbe flag */
	int SBE_flag_mask;	/* mask for sbe flag */
	int SBE_row_shift;	/* bits to shift for sbe row flag */	
	int SBE_row_mask;	/* mask for sbe register (shifted) */
	int MBE_err_address1;	/* pci offset for mbe address register */
	int MBE_err_shift1;	/* bits to shift for mbe address register */
	int MBE_err_address2;	/* pci offset for mbe address register */
	int MBE_err_shift2;	/* bits to shift for mbe address register */
	u32 MBE_err_mask;	/* mask for mbe address register */
	int MBE_err_flag;	/* MBE error flag */
	int MBE_err_row;	/* MBE row */
	u32 MBE_addr;		/* address of last MBE */
	int SBE_err_address1;	/* pci offset for mbe address register */
	int SBE_err_shift1;	/* bits to shift for mbe address register */
	int SBE_err_address2;	/* pci offset for mbe address register */
	int SBE_err_shift2;	/* bits to shift for mbe address register */
	u32 SBE_err_mask;	/* mask for mbe address register */
	int SBE_err_flag;	/* SBE error flag */
	int SBE_err_row;	/* SBE row */
	u32 SBE_addr;		/* address of last SBE */
} cs;

static inline unsigned int pci_byte(int offset)
{
	u8 value;
	pci_read_config_byte(bridge, offset, &value);
	return value & 0xFF;
}

static inline unsigned int pci_word(int offset)
{
	u16 value;
	pci_read_config_word(bridge, offset, &value);
	return value;
}

static inline unsigned int pci_dword(int offset)
{
	u32 value;
	pci_read_config_dword(bridge, offset, &value);
	return value;
}

/* write all or some bits in a byte-register*/
static inline void pci_write_bits8(int offset,u8 value,u8 mask)
{ 
        if (mask != 0xff){
                u8 buf;
                pci_read_config_byte(bridge,offset, &buf);
                value &=mask;
                buf &= ~mask;
                value |= buf;
        }
        pci_write_config_byte(bridge,offset,value);
}  

static int find_row(unsigned long err_addr)
{
	int row = 0, loop;
	for(loop=0; loop < MAXBANKS; loop++) {
		if (err_addr < bank[loop].endaddr) {
		row=loop;
			break;
		}
	}
	return row;
}

/*
 *	generic ECC check routine
 *
 *	This routine assumes that the MBE and SBE error status consist of:
 *	 * one or more bits in a status byte that are non-zero on error
 *	 * zero or more bits in a status byte that encode the row
 *	It accomodates both the case where both the MBE and SBE data are
 *	packed into a single byte (all chipsets currently known to me) as
 *	well as the case where the MBE and SBE information are contained in
 *	separate locations.  The status byte is read only once for the packed
 *	case in case the status value should be altered by being read.
 */
static void generic_check(void)
{
	int status = pci_byte(cs.MBE_flag_address);
	if ((status >> cs.MBE_flag_shift) & cs.MBE_flag_mask)
	{
		int row = (status >> cs.MBE_row_shift) & cs.MBE_row_mask;
		mbe_printk("detected in DRAM row %d\n", row);
		if (cs.MBE_err_address1)
		{
			cs.MBE_addr =
			( pci_word(cs.MBE_err_address1 << cs.MBE_err_shift1) |
			  pci_word(cs.MBE_err_address2 << cs.MBE_err_shift2) ) &
			  cs.MBE_err_mask;
			mbe_printk("at memory address %lx\n", (long unsigned int)cs.MBE_addr);
		}
		scrub_needed = 2;
		scrub_row = row;
		bank[row].mbecount++;
	}
	if (cs.SBE_flag_address != cs.MBE_flag_address)
		status = pci_byte(cs.SBE_flag_address);
	if ((status >> cs.SBE_flag_shift) & cs.SBE_flag_mask)
	{
		int row = (status >> cs.SBE_row_shift) & cs.SBE_row_mask;
		sbe_printk("detected in DRAM row %d\n", row);
		if (cs.SBE_err_address1)
		{
			cs.SBE_addr =
			( pci_word(cs.SBE_err_address1 << cs.SBE_err_shift1) |
			  pci_word(cs.SBE_err_address2 << cs.SBE_err_shift2) ) &
			  cs.SBE_err_mask;
			sbe_printk("at memory address %lx\n", (long unsigned int)cs.SBE_addr);
		}
		scrub_needed = 1;
		scrub_row = row;
		bank[row].sbecount++;
	}
}

/* VIA specific error clearing */
static void clear_via_err(void)
{
	pci_write_bits8(0x6f,0xff,0xff);
	/* as scrubbing is unimplemented, we reset it here */
	scrub_needed = 0;
}

/* unified VIA probe */
static void probe_via(void)
{
	int loop, ecc_ctrl, dimmslots = 3, bankshift = 23;
	int m_mem[] = { BANK_FPM, BANK_EDO, BANK_DDR, BANK_SDR };
	switch (device) {
		case 0x0391: /* VIA VT8371   - KX133		*/
			dimmslots = 4;
			bankshift = 24;
			bank[6].endaddr=(unsigned long)pci_byte(0x56)<<24;
			bank[7].endaddr=(unsigned long)pci_byte(0x57)<<24;
		case 0x0595: /* VIA VT82C595 - VP2,VP2/97	*/
			m_mem[2] = BANK_RESERVED;
			cs.ecc_cap = ECC_CORRECT;
			break;
		case 0x0501: /* VIA VT8501   - MVP4		*/
		case 0x0597: /* VIA VT82C597 - VP3		*/
		case 0x0598: /* VIA VT82C598 - MVP3		*/
			cs.ecc_cap = ECC_CORRECT;
			break;
		case 0x0694: /* VIA VT82C694XDP - Apollo PRO133A smp */
			dimmslots = 4;
		case 0x0691: /* VIA VT82C691 - Apollo PRO 	*/
			     /* VIA VT82C693A - Apollo Pro133 (rev 40?) */
			     /* VIA VT82C694X - Apollo Pro133A (rev 8x,Cx)
				has 4 dimm slots */
		case 0x0693: /* VIA VT82C693 - Apollo PRO-Plus 	*/
			bankshift = 24;
			cs.ecc_cap = ECC_CORRECT;
			break;
		case 0x0305: /* VIA VT8363   - KT133 - no ecc!!	*/
			     /* VIA VT8363A  - KT133A (rev 8) - no ecc!! */
			bankshift = 24;
			cs.ecc_cap = ECC_NONE;
			break;
		case 0x0585: /* VIA VT82C585 - VP,VPX,VPX/97	*/
		default:
			cs.ecc_cap = ECC_NONE;
			return;
	}
	ecc_ctrl = pci_byte(0x6E);
	cs.ecc_mode = (ecc_ctrl>>7)&1 ? cs.ecc_cap : ECC_NONE;

	cs.check = generic_check;
	if (cs.ecc_mode != ECC_NONE) {
		cs.clear_err = clear_via_err;
		/* clear initial errors */
		cs.clear_err();
	}
	cs.MBE_flag_address = 0x6F;
	cs.MBE_flag_shift = 7;
	cs.MBE_flag_mask = 1;
	cs.MBE_row_shift = 4;
	cs.MBE_row_mask = 7;
	cs.SBE_flag_address = 0x6F;
	cs.SBE_flag_shift = 3;
	cs.SBE_flag_mask = 1;
	cs.SBE_row_shift = 0;
	cs.SBE_row_mask = 7;

	for(loop=0;loop<6;loop++)
                bank[loop].endaddr = (unsigned long)pci_byte(0x5a+loop)<<bankshift;
	for(loop=0;loop<dimmslots;loop++)
	{
		bank[loop*2].mtype = m_mem[(pci_byte(0x60)>>(loop*2))&3];
		bank[(loop*2)+1].mtype = m_mem[(pci_byte(0x60)>>(loop*2))&3];
		bank[loop*2].eccmode = (ecc_ctrl>>(loop))&1;
		bank[(loop*2)+1].eccmode = (ecc_ctrl>>(loop))&1;
	}
}

static void check_serverworks(void)
{
	unsigned long mesr;
	int row;

	mesr = pci_dword(0x94); /* mesr0 */
	row = (int)(mesr>>29)&0x7;
	if((mesr>>31)&1)
	{
		mbe_printk("Detected in DRAM row %d\n", row);
		scrub_needed |= 2;
		bank[row].mbecount++;
	}
	if((mesr>>30)&1)
	{
		sbe_printk("Detected in DRAM row %d\n", row);
		scrub_needed |= 1;
		bank[row].sbecount++;
	}
	if (scrub_needed)
	{
		/*
		 * clear error flag bits that were set by writing 1 to them
		 * we hope the error was a fluke or something :)
		 */
		unsigned long value = scrub_needed<<30;
		pci_write_config_dword(bridge, 0x94, value);
		scrub_needed = 0;
	}

	mesr = pci_dword(0x98); /* mesr1 */
	row = (int)(mesr>>29)&0x7;
	if((mesr>>31)&1)
	{
		mbe_printk("Detected in DRAM row %d\n", row);
		scrub_needed |= 2;
		bank[row].mbecount++;
	}
	if((mesr>>30)&1)
	{
		sbe_printk("Detected in DRAM row %d\n", row);
		scrub_needed |= 1;
		bank[row].sbecount++;
	}
	if (scrub_needed)
	{
		/*
		 * clear error flag bits that were set by writing 1 to them
		 * we hope the error was a fluke or something :)
		 */
		unsigned long value = scrub_needed<<30;
		pci_write_config_dword(bridge, 0x98, value);
		scrub_needed = 0;
        }
}

static void probe_serverworks(void)
{
	int loop, efcr, mccr;
	switch (device) {
		case 0x0008: /* serverworks iii he */
		case 0x0009: /* serverworks iii le */
			cs.ecc_cap = ECC_AUTO;
			break;
		default:
			cs.ecc_cap = ECC_NONE;
			return;
	}
	efcr = pci_byte(0xE0);
	mccr = pci_word(0x92);
	cs.ecc_mode = (efcr>>2)&1 ? (mccr&1 ? ECC_AUTO : ECC_CORRECT) : ECC_NONE;

	cs.check = check_serverworks;

	for(loop=0;loop<8;loop++)
	{
		bank[loop].mtype = BANK_UNKNOWN;
		bank[loop].eccmode = cs.ecc_mode;
                bank[loop].endaddr = 0;
	}
}

/*
 * 450gx probing is buggered at the moment. help me obi-wan.
 */
static void probe_450gx(void)
{
	int loop, dramc, merrcmd;
	u32 nbxcfg;
	int m_mem[] = { BANK_EDO, BANK_SDR, BANK_RDR, BANK_RESERVED };
/*	int ddim[] = { ECC_NONE, ECC_DETECT, ECC_CORRECT, ECC_AUTO }; */
	nbxcfg = pci_dword(0x50);
	dramc = pci_byte(0x57);
	merrcmd = pci_word(0xC0);
	for(loop=0;loop<8;loop++)
	{
		bank[loop].endaddr=(unsigned long)pci_byte(0x60+loop)<<23;
		/* 450gx doesnt allow mixing memory types. bleah. */
		bank[loop].mtype = m_mem[(dramc>>3)&3];
		/* yes, bit is _zero_ if ecc is _enabled_. */
		bank[loop].eccmode = !((nbxcfg>>(loop+24))&1);
	}
	cs.ecc_cap = ECC_AUTO;
	cs.ecc_mode = (merrcmd>>1)&1 ? ECC_AUTO : ECC_DETECT;

        cs.check = generic_check;
        cs.MBE_flag_address = 0xC2;
        cs.MBE_flag_shift = 0;
        cs.MBE_flag_mask = 1;
        cs.MBE_row_shift = 5;
        cs.MBE_row_mask = 7;
        cs.SBE_flag_address = 0xC2;
        cs.SBE_flag_shift = 1;
        cs.SBE_flag_mask = 1;
        cs.SBE_row_shift = 1;
        cs.SBE_row_mask = 7;

	cs.MBE_err_address1 = 0xA8;
	cs.MBE_err_shift1 = 0;
	cs.MBE_err_address2 = 0xAA;
	cs.MBE_err_shift2 = 16;
	cs.MBE_err_mask = 0xFFFFFFFC;

	cs.SBE_err_address1 = 0x74;
	cs.SBE_err_shift1 = 0;
	cs.SBE_err_address2 = 0x76;
	cs.SBE_err_shift2 = 16;
	cs.SBE_err_mask = 0xFFFFFFFC;
}

/* there seems to be NO WAY to distinguish 440zx from 440bx!! >B( */
static void probe_440bx(void)
{
	int loop, dramc, errcmd;
	u32 nbxcfg;
	int m_mem[] = { BANK_EDO, BANK_SDR, BANK_RDR, BANK_RESERVED };
	int ddim[] = { ECC_NONE, ECC_DETECT, ECC_CORRECT, ECC_AUTO };
	nbxcfg = pci_dword(0x50);
	dramc = pci_byte(0x57);
	errcmd = pci_byte(0x90);
	cs.ecc_cap = ECC_AUTO;
	cs.ecc_mode = ddim[(nbxcfg>>7)&3];

        cs.check = generic_check;
        cs.MBE_flag_address = 0x91;
        cs.MBE_flag_shift = 4;
        cs.MBE_flag_mask = 1;
        cs.MBE_row_shift = 5;
        cs.MBE_row_mask = 7;
        cs.SBE_flag_address = 0x91;
        cs.SBE_flag_shift = 0;
        cs.SBE_flag_mask = 1;
        cs.SBE_row_shift = 1;
        cs.SBE_row_mask = 7;

	cs.MBE_err_address1 = 80;
	cs.MBE_err_shift1 = 0;
	cs.MBE_err_address2 = 82;
	cs.MBE_err_shift2 = 16;
	cs.MBE_err_mask = 0xFFFFF000;

	cs.SBE_err_address1 = 80;
	cs.SBE_err_shift1 = 0;
	cs.SBE_err_address2 = 82;
	cs.SBE_err_shift2 = 16;
	cs.SBE_err_mask = 0xFFFFF000;

	for(loop=0;loop<8;loop++)
	{
		bank[loop].endaddr=(unsigned long)pci_byte(0x60+loop)<<23;
		/* 440bx doesnt allow mixing memory types. bleah. */
		bank[loop].mtype = m_mem[(dramc>>3)&3];
		/* yes, bit is _zero_ if ecc is _enabled_. */
		bank[loop].eccmode = !((nbxcfg>>(loop+24))&1);
	}
}

/* no way to tell 440ex from 440lx!? grr. */
static void probe_440lx(void)
{
	int loop, drt, paccfg, errcmd;
	int m_mem[] = { BANK_EDO, BANK_RESERVED, BANK_SDR, BANK_EMPTY };
	int ddim[] = { ECC_NONE, ECC_DETECT, ECC_RESERVED, ECC_CORRECT } ;
	paccfg = pci_word(0x50);
	drt = pci_byte(0x55) | (pci_byte(0x56)<<8);
	errcmd = pci_byte(0x90);
	/* 440ex doesnt support ecc, but no way to tell if its 440ex! */
	cs.ecc_cap = ECC_CORRECT;
	cs.ecc_mode = ddim[(paccfg>>7)&3];

        cs.check = generic_check;
        cs.MBE_flag_address = 0x91;
        cs.MBE_flag_shift = 4;
        cs.MBE_flag_mask = 1;
        cs.MBE_row_shift = 5;
        cs.MBE_row_mask = 7;
        cs.SBE_flag_address = 0x91;
        cs.SBE_flag_shift = 0;
        cs.SBE_flag_mask = 1;
        cs.SBE_row_shift = 1;
        cs.SBE_row_mask = 7;

	for(loop=0;loop<8;loop++)
	{
		bank[loop].endaddr = (unsigned long)pci_byte(0x60+loop)<<23;
		bank[loop].mtype = m_mem[(drt>>(loop*2))&3];
		bank[loop].eccmode = (cs.ecc_mode != 0);
	}
}

static void probe_440fx(void)
{
	int loop, drt, pmccfg, errcmd;
	int m_mem[] = { BANK_FPM, BANK_EDO, BANK_BEDO, BANK_EMPTY };
	int ddim[] = { ECC_NONE, ECC_PARITY, ECC_DETECT, ECC_CORRECT };
	pmccfg = pci_word(0x50);
	drt = pci_byte(0x55) | (pci_byte(0x56)<<8);
	errcmd = pci_byte(0x90);
	for(loop=0;loop<8;loop++)
	{
		bank[loop].endaddr=(unsigned long)pci_byte(0x60+loop)<<23;
		bank[loop].mtype = m_mem[(drt>>(loop*2))&3];
	}
	cs.ecc_cap = ECC_CORRECT;
	cs.ecc_mode = ddim[(pmccfg>>4)&3];

        cs.check = generic_check;
        cs.MBE_flag_address = 0x91;
        cs.MBE_flag_shift = 4;
        cs.MBE_flag_mask = 1;
        cs.MBE_row_shift = 5;
        cs.MBE_row_mask = 7;
        cs.SBE_flag_address = 0x91;
        cs.SBE_flag_shift = 0;
        cs.SBE_flag_mask = 1;
        cs.SBE_row_shift = 1;
        cs.SBE_row_mask = 7;
}

static void probe_430hx(void)
{
	int pcicmd, pcon, errcmd, drt, loop;
	pcicmd = pci_word(0x4);
	pcon = pci_byte(0x50);
	drt = pci_byte(0x68);
	errcmd = pci_byte(0x90);
	cs.ecc_cap = ECC_CORRECT;
	cs.ecc_mode = (pcon>>7)&1 ? ECC_CORRECT : ECC_PARITY;

        cs.check = generic_check;
        cs.MBE_flag_address = 0x91;
        cs.MBE_flag_shift = 4;
        cs.MBE_flag_mask = 1;
        cs.MBE_row_shift = 5;
        cs.MBE_row_mask = 7;
        cs.SBE_flag_address = 0x91;
        cs.SBE_flag_shift = 0;
        cs.SBE_flag_mask = 1;
        cs.SBE_row_shift = 1;
        cs.SBE_row_mask = 7;

	for(loop=0;loop<8;loop++)
	{
		bank[loop].endaddr=(unsigned long)pci_byte(0x60+loop)<<22;
		bank[loop].mtype = (drt>>loop)&1 ? BANK_EDO : BANK_FPM;
		bank[loop].eccmode = cs.ecc_mode;
	}
}

static void check_840(void)
{
       int errsts = pci_word(0xE2);
       if ((errsts>>9) & 1)
       {
               u32 eap = pci_dword(0xE4) & 0xFFFFF800;
               mbe_printk("at memory address %lx\n row %d syndrome %x",
                       (long unsigned int)eap, find_row(eap), errsts&0xFF);
               scrub_needed = 2;
               scrub_row = find_row(eap);
               bank[scrub_row].mbecount++;
       }
       if ((errsts>>10) & 1)
       {
               u32 eap = pci_dword(0xE4) & 0xFFFFF800;
               sbe_printk("at memory address %lx row %d syndrome %x\n",
                       (long unsigned int)eap, find_row(eap), errsts&0xFF);
               scrub_needed = 1;
               scrub_row = find_row(eap);
               bank[scrub_row].sbecount++;
       }
}

static void probe_840(void)
{
       int loop, mchcfg;
       int ddim[] = { ECC_NONE, ECC_RESERVED, ECC_CORRECT, ECC_AUTO };
       mchcfg = pci_word(0x50);
       cs.ecc_cap = ECC_AUTO;
       cs.ecc_mode = ddim[(mchcfg>>7)&3];

       cs.check = check_840;

       for(loop=0;loop<16;loop++)
       {
               bank[loop].endaddr=(unsigned long)pci_word(0x60+(loop*2))<<23;
               bank[loop].mtype = BANK_RMBS;
               bank[loop].eccmode = cs.ecc_mode;
       }
}

static void check_845(void)
{
       int errsts = pci_word(0xC8);
       if ((errsts>>1) & 1)
       {
               u32 eap = (pci_dword(0x8C) & 0xFFFFFFFE)<<4;
               char derrsyn = pci_byte(0x86);
               mbe_printk("at memory address %lx\n row %d syndrome %x",
                       (long unsigned int)eap, find_row(eap), derrsyn);
               scrub_needed = 2;
               scrub_row = find_row(eap);
               bank[scrub_row].mbecount++;
       }
       if (errsts & 1)
       {
               u32 eap = (pci_dword(0x8C) & 0xFFFFFFFE)<<4;
               char derrsyn = pci_byte(0x86);
               sbe_printk("at memory address %lx row %d syndrome %x\n",
                       (long unsigned int)eap, find_row(eap), derrsyn);
               scrub_needed = 1;
               scrub_row = find_row(eap);
               bank[scrub_row].sbecount++;
       }
}

static void probe_845(void)
{
       int loop;
       u32 drc;
       int ddim[] = { ECC_NONE, ECC_RESERVED, ECC_CORRECT, ECC_RESERVED };
       drc = pci_dword(0x7C);
       cs.ecc_cap = ECC_CORRECT;
       cs.ecc_mode = ddim[(drc>>19)&3];

       cs.check = check_845;

       for(loop=0;loop<8;loop++)
       {
               bank[loop].endaddr=(unsigned long)pci_byte(0x60+loop)<<24;
               bank[loop].mtype = BANK_SDR;
               bank[loop].eccmode = cs.ecc_mode;
       }
}

static void check_850(void)
{
       int errsts = pci_word(0xC8);
       if ((errsts>>1) & 1)
       {
               u32 eap = pci_dword(0xE4) & 0xFFFFE000;
               char derrsyn = pci_word(0xE2) & 0x7F;
               mbe_printk("at memory address %lx\n row %d syndrome %x",
                       (long unsigned int)eap, find_row(eap), derrsyn);
               scrub_needed = 2;
               scrub_row = find_row(eap);
               bank[scrub_row].mbecount++;
       }
       if (errsts & 1)
       {
               u32 eap = pci_dword(0xE4) & 0xFFFFE000;
               char derrsyn = pci_byte(0xE2) & 0x7F;
               sbe_printk("at memory address %lx row %d syndrome %x\n",
                       (long unsigned int)eap, find_row(eap), derrsyn);
               scrub_needed = 1;
               scrub_row = find_row(eap);
               bank[scrub_row].sbecount++;
       }
}

static void probe_850(void)
{
       int loop;
       int mchcfg;
       int ddim[] = { ECC_NONE, ECC_RESERVED, ECC_CORRECT, ECC_RESERVED };
       mchcfg = pci_word(0x50);
       cs.ecc_cap = ECC_CORRECT;
       cs.ecc_mode = ddim[(mchcfg>>7)&3];

       cs.check = check_850;

       for(loop=0;loop<16;loop++)
       {
               bank[loop].endaddr=(unsigned long)pci_word(0x60+(loop*2))<<23;
               bank[loop].mtype = BANK_RMBS;
               bank[loop].eccmode = cs.ecc_mode;
       }
}

/* 860 specific error clearing */
static void clear_860_err(void)
{
       pci_write_bits8(0xc8,0x03,0x03);
       /* as scrubbing is unimplemented, we reset it here */
       scrub_needed = 0;
       cs.clear_err = 0;
}

static void check_860(void)
{
       int errsts = pci_word(0xC8);
       if ((errsts>>1) & 1)
       {
               u32 eap = (pci_dword(0xE4) & 0xFFFFFE00)<<2;
               int channel = pci_dword(0xE4) & 1;
               unsigned char derrsyn = pci_word(0xE2) & 0xFF;
               mbe_printk("at memory address %08lx row %d syndrome %02x channel=%s\n",
                       (long unsigned int)eap, find_row(eap), derrsyn, channel==0?"A":"B");
               scrub_needed = 2;
               scrub_row = find_row(eap);
               bank[scrub_row].mbecount++;
               cs.clear_err = clear_860_err;
       }
       if (errsts & 1)
       {
	       u32 eap = (pci_dword(0xE4) & 0xFFFFFE00)<<2;
               int channel = pci_dword(0xE4) & 1;
               unsigned char derrsyn = pci_byte(0xE2) & 0xFF;
               sbe_printk("at memory address %08lx row %d syndrome %02x channel=%s\n",
                       (long unsigned int)eap, find_row(eap), derrsyn, channel==0?"A":"B");
               scrub_needed = 1;
               scrub_row = find_row(eap);
               bank[scrub_row].sbecount++;
               cs.clear_err = clear_860_err;
       }
       /* 
        * We should catch other error bits, however we need to add code
        * to clear them if we want to continue on (otherwise we will report
        * the same error on every poll).  Having never seen other bits set
        * on our systems, just panic for now. -jg
        */
       if (errsts & 0xfc)
               panic("ECC: MCH ERRSTS register contains 0x%x\n", errsts);
}

#if    I860_DUMPCONFIG
void info_860(void)
{
       int i;

       printk("RDRAM Device Group Architecture Registers (16): \n");
       for (i = 0; i < 16; i++) {
               printk("GAR[%d]=%-2.2x ", i, pci_byte(0x40 + i));
               if ((i + 1) % 4 == 0)
                       printk("\n");
       }
       printk("MCH Configuration Register: MCHCFG=%-4.4x\n", pci_word(0x50));
       printk("Fixed DRAM Hole Control Register: FDHC=%-2.2x\n", 
                       pci_byte(0x58));
       printk("Programmable Attribute Map Registers (7):\n");
       for (i = 0; i < 7; i++) {
               printk("PAM[%d]=%-2.2x ", i, pci_byte(0x59 + i));
               if ((i + 1) % 4 == 0)
                       printk("\n");
       }
       printk("\n");
       printk("RDRAM Device Pool Sizing Register: RDPS=%-2.2x\n", 
                       pci_byte(0x88));
       printk("RDRAM Device Init Control Management Register: RICM=%-8.8x\n", 
                       pci_dword(0x94));
       printk("System Management RAM Control Register: SMRAM=%-2.2x\n",
                       pci_byte(0x9D));
       printk("Extended System Mgmt RAM Control Register: ESMRAMC=%-2.2x\n",
                       pci_byte(0x9E));
       printk("RDRAM Device Timing Register: RDTR=%-2.2x\n", pci_byte(0xBE));
       printk("Top of Low Memory Register: TOM=%-4.4x\n", pci_word(0xC4));
       printk("Error Command Register: ERRCMD=%-4.4x\n", pci_word(0xCA));
       printk("SMI Command Register: SCICMD=%-4.4x\n", pci_word(0xCC));
       printk("SCI Command Register: SCICMD=%-4.4x\n", pci_word(0xCE));
       printk("RDRAM Device Refresh Control Register: DRAMRC=%-4.4x\n",
                       pci_word(0xDC));
       printk("Misc. Control Register: MISC_CNTL=%-8.8x\n", pci_dword(0xF4));
       printk("PCI Command Register: PCICMD=%-4.4x\n", pci_word(0x04));
}
#else
void info_860(void)
{
       printk("ECC: Intel 860 MCH probed successfully\n");
}
#endif

static void probe_860(void)
{
       int loop;
       u32 drc;
       int ddim[] = { ECC_NONE, ECC_RESERVED, ECC_CORRECT, ECC_RESERVED };
       drc = pci_word(0x50);
       cs.ecc_cap = ECC_CORRECT;
       cs.ecc_mode = ddim[(drc>>7)&3];

       cs.check = check_860;

       for(loop=0;loop<MAXBANKS;loop++)
       {
               bank[loop].endaddr=(unsigned long)pci_word(0x60+(loop*2))<<24;
               bank[loop].mtype = BANK_RMBS;
               bank[loop].eccmode = cs.ecc_mode;
               if (loop > 0) { /* find_bank assumes ascending addresses */
                       assert(bank[loop].endaddr >= bank[loop-1].endaddr);
       }
       }
       clear_860_err(); /* start off with a clean slate */
       info_860();
}

static void check_amd751(void)
{
	int eccstat = pci_word(0x58);
        int csbits = eccstat & 0x3F;
        int row;
        switch (csbits)
	{
		case 1: row = 0; break;
		case 2: row = 1; break;
		case 4: row = 2; break;
		case 8: row = 3; break;
		case 16: row = 4; break;
		case 32: row = 5; break;
		default: row = 6;
	}
	if(((eccstat>>8)&3) == 1)
	{
		mbe_printk("Detected in DRAM row %d\n", row);
		scrub_needed=2;
		bank[row].mbecount++;
	}
	if(((eccstat>>8)&3) == 2)
	{
		sbe_printk("Detected in DRAM row %d\n", row);
		scrub_needed=1;
		bank[row].sbecount++;
	}
	if(((eccstat>>8)&3) == 3)
	{
		mbe_printk("and SBE Detected in DRAM row %d\n", row);
		scrub_needed=1;
		bank[row].sbecount++;
	}
	if (scrub_needed)
	{
		/*
		 * clear error flag bits that were set by writing 0 to them
		 * we hope the error was a fluke or something :)
		 */
		int value = eccstat & 0xFCFF;
		pci_write_config_word(bridge, 0x58, value);
		scrub_needed = 0;
        }
}

static void probe_amd751(void)
{
	int loop;
	cs.ecc_cap = ECC_CORRECT;
	cs.ecc_mode = (pci_byte(0x5a)>>2)&1 ? ECC_CORRECT : ECC_NONE;
	cs.check = check_amd751;
	for(loop=0;loop<6;loop++)
	{
		unsigned flag = pci_byte(0x40+loop*2);
		/* bank address mask. */
		unsigned mask = (flag&0x7F)>>1;
		/* actually starting address */
		bank[loop].endaddr=(unsigned long)(pci_word(0x40+loop*2)&0xFF80)<<(23-7);
		/* only when bank is populated */
		if((flag&1)&&(mask!=0)){
			/* mask+1 * 8mb appears to be bank size */
			bank[loop].endaddr += (mask + 1) * 8 * (1024 * 1024); /* -1? */
		}
		bank[loop].mtype = flag&1 ? BANK_SDR : BANK_EMPTY;
		/* no per-bank register, assumed same for all banks? */
		bank[loop].eccmode = (pci_byte(0x5a)>>2)&1;
	}
}

static void check_amd76x(void)
{
	unsigned long eccstat = pci_dword(0x48);
	if(eccstat & 0x10)
	{
		/* bits 7-4 of eccstat indicate the row the MBE occurred. */
		int row = (eccstat >> 4) & 0xf;
		mbe_printk("Detected in DRAM row %d\n", row);
		scrub_needed |= 2;
		bank[row].mbecount++;
	}
	if(eccstat & 0x20)
	{
		/* bits 3-0 of eccstat indicate the row the SBE occurred. */
		int row = eccstat & 0xf;
		sbe_printk("Detected in DRAM row %d\n", row);
		scrub_needed |= 1;
		bank[row].sbecount++;
	}
	if (scrub_needed)
	{
		/*
		 * clear error flag bits that were set by writing 0 to them
		 * we hope the error was a fluke or something :)
		 */
		unsigned long value = eccstat;
		if (scrub_needed & 1)
			value &= 0xFFFFFDFF;
		if (scrub_needed & 2)
			value &= 0xFFFFFEFF;
		pci_write_config_dword(bridge, 0x48, value);
		scrub_needed = 0;
        }
}

static void probe_amd76x(void)
{
	static const int modetab[] = {ECC_NONE, ECC_DETECT, ECC_CORRECT, ECC_AUTO};
	int loop;
	unsigned long addr = 0;
	cs.ecc_cap = ECC_AUTO;
	cs.ecc_mode = modetab [(pci_dword(0x48)>>10)&3];
	cs.check = check_amd76x;

	/* create fake end addresses, as the chipset is capable of
	   matching addresses to banks in random order */
	for(loop=0;loop<8;loop++)
	{
		unsigned long r = pci_dword(0xc0+(loop*4));
		bank[loop].mtype = r & 1 ? BANK_DDR : BANK_EMPTY;
		if (r & 1)
			addr += ((r & 0xff80) << 16) + 0x800000;
		bank[loop].endaddr=addr;
		/* no per-bank register, assumed same for all banks? */
		bank[loop].eccmode = cs.ecc_mode != ECC_NONE;
	}
}

/* SiS */
void probe_sis(void)
{
	int loop;
	u32 endaddr;
	int m_mem[] = { BANK_FPM, BANK_EDO, BANK_RESERVED, BANK_SDR };
	int dramsize[] = { 256, 1024, 4096, 16384, 1024, 2048, 4096,
		8192, 512, 1024, 2048, 0, 0, 0, 0, 0 };
	int sdramsize[] = { 1024, 4096, 4096, 8192, 2048, 8192,
		8192, 16384, 4096, 16384, 16384, 32768, 2048, 0, 0, 0 };

	cs.ecc_cap = ECC_CORRECT;

        cs.check = generic_check;

        cs.MBE_flag_address = 0x64;
        cs.MBE_flag_shift = 4;
        cs.MBE_flag_mask = 1;
        cs.MBE_row_shift = 5;
        cs.MBE_row_mask = 7;

        cs.SBE_flag_address = 0x64;
        cs.SBE_flag_shift = 3;
        cs.SBE_flag_mask = 1;
        cs.SBE_row_shift = 5;
        cs.SBE_row_mask = 7;

	cs.MBE_err_address1 = 0x64;
	cs.MBE_err_shift1 = 0;
	cs.MBE_err_address2 = 0x66;
	cs.MBE_err_shift2 = 16;
	cs.MBE_err_mask = 0xFFFFF000;

	cs.SBE_err_address1 = 0x64;
	cs.SBE_err_shift1 = 0;
	cs.SBE_err_address2 = 0x66;
	cs.SBE_err_shift2 = 16;
	cs.MBE_err_mask = 0xFFFFF000;

	endaddr = 0;
	for(loop=0;loop<3;loop++)
	{
		/* populated bank? */
		if ((pci_byte(0x63)>>loop)&1)
		{
			u32 banksize;
			int mtype = pci_byte(0x60+loop);

			bank[loop*2].mtype = m_mem[(mtype>>6)&3];
			if(bank[loop*2].mtype == BANK_SDR)
			{
				banksize = sdramsize[mtype&15]*1024;
			} else {
				banksize = dramsize[mtype&15]*1024;
			}
			endaddr += banksize;
			bank[loop*2].endaddr = endaddr;
			/* double sided dimm? */
			if ((mtype>>5)&1)
			{
				bank[(loop*2)+1].mtype = bank[loop*2].mtype;
				endaddr += banksize;
				bank[(loop*2)+1].endaddr = endaddr;
			}
		} else {
			bank[loop*2].mtype = BANK_EMPTY;
			bank[(loop*2)+1].mtype = BANK_EMPTY;
			bank[loop*2].endaddr = endaddr;
			bank[(loop*2)+1].endaddr = endaddr;
		}
	}
	cs.ecc_mode = ECC_NONE;
	for(loop=0;loop<6;loop++)
	{
		int eccmode = (pci_byte(0x74)>>loop)&1;
		bank[loop].eccmode = eccmode;
		if(eccmode)
			cs.ecc_mode = ECC_CORRECT;
	}
}

/* ALi */
void probe_aladdin4(void)
{
	int loop;
	int m_mem[] = { BANK_FPM, BANK_EDO, BANK_RESERVED, BANK_SDR };
	cs.ecc_cap = ECC_CORRECT;
	cs.ecc_mode = pci_byte(0x49)&1 ? ECC_CORRECT : ECC_PARITY;

        cs.check = generic_check;

        cs.MBE_flag_address = 0x4a;
        cs.MBE_flag_shift = 4;
        cs.MBE_flag_mask = 1;
        cs.MBE_row_shift = 5;
        cs.MBE_row_mask = 7;

        cs.SBE_flag_address = 0x4a;
        cs.SBE_flag_shift = 0;
        cs.SBE_flag_mask = 1;
        cs.SBE_row_shift = 1;
        cs.SBE_row_mask = 7;

	for(loop=0;loop<8;loop++)
	{
		bank[loop].endaddr = (unsigned long)(pci_byte(0x61+(loop*2))&15)<<27|(pci_byte(0x60+(loop*2))<<20);
		bank[loop].mtype = m_mem[(pci_byte(0x61+(loop*2))>>1)&3];
		if (cs.ecc_mode == ECC_CORRECT) {
			bank[loop].eccmode = 1;
		} else {
			bank[loop].eccmode = 0;
		}
	}
}

void probe_aladdin5(void)
{
	int loop;
	int m_mem[] = { BANK_FPM, BANK_EDO, BANK_RDR, BANK_SDR };
	cs.ecc_cap = ECC_CORRECT;
	cs.ecc_mode = pci_byte(0x50)&1 ? ECC_CORRECT : ECC_PARITY;

        cs.check = generic_check;

        cs.MBE_flag_address = 0x51;
        cs.MBE_flag_shift = 4;
        cs.MBE_flag_mask = 1;
        cs.MBE_row_shift = 5;
        cs.MBE_row_mask = 7;

        cs.SBE_flag_address = 0x51;
        cs.SBE_flag_shift = 0;
        cs.SBE_flag_mask = 1;
        cs.SBE_row_shift = 1;
        cs.SBE_row_mask = 7;

	for(loop=0;loop<8;loop++)
	{
		/* DBxCII not disabled address mapping? */
		if(pci_byte(0x61+(loop*2))&0xF0)
		{
			/* endaddr always 1 unit low, granularity 1mb */
			bank[loop].endaddr = (unsigned long)((pci_byte(0x61+(loop*2))&15)<<27|(pci_byte(0x60+(loop*2))<<20))+1048576;
			bank[loop].mtype = m_mem[(pci_byte(0x61+(loop*2))>>1)&3];
			if (cs.ecc_mode == ECC_CORRECT) {
				bank[loop].eccmode = 1;
			} else {
				bank[loop].eccmode = 0;
			}
		}
	}
}

#if 0
/*
 * memory scrubber routines, not ready to be used yet...
 */
/* start at 16mb */
unsigned long start = 4096;
unsigned long pages = 1;
/* other architectures have different page sizes... */
unsigned long step = 4096;

char buff[8192] = {0,};

/*
 * Michael's page scrubber routine
 */
void scrub_page(unsigned long volatile * p)
{
	int i;
	int len, err = 0;
	unsigned long *q;
	q = (unsigned long *) ((((int)buff)+4095) & ~4095);

	if (((int)p) >= 640 * 1024 && ((int)p) < 1024 * 1024)
		return;

	cli();	/* kill interrupts */
	err = pci_byte(0x91);
	outb(0x11, PCI_DATA + 1); /* clear the memory error indicator */
	
	for (i = 0; i < step / 4 ; ++i)
		q[i] = p[i];
	for (i = 0; i < step / 4 ; ++i)
		p[i] = q[i];
	err = inb(PCI_DATA + 1);
	sti();
	if (err & 0x11) {
		printk(KERN_ERR "ECC: Memory error @ %08x (0x%02x)\n", p, err);
		return 1;
	}
	return 0;
}

void scrub(void)
{
	int i,j = 0;
	for (i = 0; i < pages; ++i) {
		j = scrub_page(start);
		start += step;
	}
	if (!j) {
		/*
		 * Hmm... This is probably a very bad situation.
		 */
		printk(KERN_ERR "ECC: Scrubbed, no errors found?!\n");
		scrub_needed=0;
		return;
	}
	if (scrub_needed==2) {
		/*
		 * TODO: We should determine what process owns the memory
		 * and send a SIGBUS to it. We should also printk something
		 * along the lines of
		 * "ECC: Process (PID) killed with SIGBUS due to uncorrectable memory error at 0xDEADBEEF"
		 */
		scrub_needed=0;
	}
}
#endif

int unload = 0;
/*
 * Check ECC status every second.
 * SMP safe, doesn't use NMI, and auto-rate-limits.
 */
void checkecc(void) {
        MOD_INC_USE_COUNT;
	if (!scrub_needed)
		if (cs.check)
			cs.check();
	if (cs.clear_err)
		cs.clear_err();
	init_timer(&ecctimer);
	ecctimer.expires = jiffies + (HZ * poll_msec) / 1000;
	ecctimer.function = (void *)&checkecc;
	if (!unload)
	        add_timer(&ecctimer);
	MOD_DEC_USE_COUNT;
}

#ifdef CONFIG_PROC_FS
int procfile_read(char *memstat, char **buffer_location, off_t offset,
	int buffer_length, int *eof, void *data)
{
	char *ecc[] = { "None", "Reserved", "Parity checking", "ECC detection",
		"ECC detection and correction", "ECC with hardware scrubber" };
	char *dram[] = { "Empty", "Reserved", "Unknown", "FPM", "EDO", "BEDO", "SDR",
		"DDR", "RDR", "RMBS" };
	unsigned long mem_end = 0;
	unsigned long last_mem = 0;
	int loop;
	int len = 0;

	if (offset)
		return 0;

	len += sprintf(memstat, "Chipset ECC capability : %s\n", ecc[cs.ecc_cap]);
	len += sprintf(memstat + len, "Current ECC mode : %s\n", ecc[cs.ecc_mode]);
	len += sprintf(memstat + len, "Bank\tSize\tType\tECC\tSBE\tMBE\n");
	for (loop=0;loop<MAXBANKS;loop++){
		last_mem=bank[loop].endaddr;
		if (last_mem>mem_end) {
			len += sprintf(memstat + len, "%d\t", loop);
			len += sprintf(memstat + len, "%dM\t", (int)(last_mem-mem_end)/1048576);
			len += sprintf(memstat + len, "%s\t", dram[bank[loop].mtype]);
			len += sprintf(memstat + len, "%s\t", bank[loop].eccmode ? "Y" : "N");
			len += sprintf(memstat + len, "%ld\t", (unsigned long)bank[loop].sbecount);
			len += sprintf(memstat + len, "%ld\n", (unsigned long)bank[loop].mbecount);
			mem_end=last_mem;
		}
	}
	len += sprintf(memstat + len, "Total\t%dM\n", (int)(mem_end>>20));

	if (len <= buffer_length)
		*eof = 1;
	else
		len = buffer_length;
	return len;
}
#endif

struct pci_probe_matrix {
	int vendor;		/* pci vendor id */
	int device;		/* pci device id */
	void (*check)(void);	/* pointer to chipset probing routine */
};

static struct pci_probe_matrix probe_matrix[] = {
	/* AMD */
	{ 0x1022, 0x7006, probe_amd751 },
	{ 0x1022, 0x700c, probe_amd76x }, /* amd762 2 CPU */
	{ 0x1022, 0x700e, probe_amd76x }, /* amd761 1 CPU */
	/* Motorola */
	{ 0x1057, 0x4802, 0 }, /* falcon - not yet supported */
	/* Apple */
	{ 0x106b, 0x0001, 0 }, /* bandit - not yet supported */
	/* SiS */
	{ 0x1039, 0x0600, probe_sis }, /* 600 programatically same as 5600 */
	{ 0x1039, 0x0620, 0 }, /* 620 doesnt support ecc */
	{ 0x1039, 0x5600, probe_sis },
	/* ALi */
	{ 0x10b9, 0x1531, probe_aladdin4 },
	{ 0x10b9, 0x1541, probe_aladdin5 },
	/* VIA */
	{ 0x1106, 0x0305, probe_via }, /* vt8363 - kt133/km133 */
	{ 0x1106, 0x0391, probe_via }, /* vt8371 - kx133 */
	{ 0x1106, 0x0501, probe_via }, /* vt8501 - mvp4 */
	{ 0x1106, 0x0585, probe_via }, /* vt82c585 - vp/vpx */
	{ 0x1106, 0x0595, probe_via }, /* vt82c595 - vp2 */
	{ 0x1106, 0x0597, probe_via }, /* vt82c597 - vp3 */
	{ 0x1106, 0x0598, probe_via }, /* vt82c598 - mvp3 */
	{ 0x1106, 0x0691, probe_via }, /* vt82c691 - pro133/pro133a */
	{ 0x1106, 0x0693, probe_via }, /* vt82c693 - pro+ */
	{ 0x1106, 0x0694, probe_via }, /* vt82c694 - pro133a */
	/* Serverworks */
	{ 0x1166, 0x0008, probe_serverworks }, /* CNB20HE - serverset iii he */
	{ 0x1166, 0x0009, probe_serverworks }, /* serverset iii le */
	/* Intel */
	{ 0x8086, 0x1130, 0 }, /* 815 doesnt support ecc */
	{ 0x8086, 0x122d, 0 }, /* 430fx doesnt support ecc */
	{ 0x8086, 0x1237, probe_440fx },
	{ 0x8086, 0x1250, probe_430hx },
	{ 0x8086, 0x1A21, probe_840 },
	{ 0x8086, 0x1A30, probe_845 },
	{ 0x8086, 0x2530, probe_850 },
	{ 0x8086, 0x2531, probe_860 },
	{ 0x8086, 0x7030, 0 }, /* 430vx doesnt support ecc */
	{ 0x8086, 0x7120, 0 }, /* 810 doesnt support ecc */
	{ 0x8086, 0x7122, 0 },
	{ 0x8086, 0x7124, 0 }, /* 810e doesnt support ecc */
	{ 0x8086, 0x7180, probe_440lx }, /* also 440ex */
	{ 0x8086, 0x7190, probe_440bx }, /* also 440zx */
	{ 0x8086, 0x7192, probe_440bx }, /* also 440zx */
	{ 0x8086, 0x71A0, probe_440bx }, /* also 440gx */
	{ 0x8086, 0x71A2, probe_440bx }, /* also 440gx */
	{ 0x8086, 0x84C5, probe_450gx },
	{ 0, 0, 0 }
};

static int find_chipset(void) {

	if (!pci_present()) {
		printk(KERN_ERR "ECC: No PCI bus.\n");
		return 0;
	}

	while ((bridge = pci_find_class(PCI_CLASS_BRIDGE_HOST << 8, bridge)))
	{
		int loop = 0;
		/* Direct reading of the pci config space of these is not 
		   allowed in linux (eg it leaves no place for fixups and 
		   other items); so replacing the following two lines with 
		   those that follow immediately after it is a bugfix added by 
		   RH. - arjanv@redhat.com 3/26/2002
                pci_read_config_word(bridge, PCI_VENDOR_ID, &vendor);
                pci_read_config_word(bridge, PCI_DEVICE_ID, &device);
		*/
		vendor = bridge->vendor;
		device = bridge->device;
		while(probe_matrix[loop].vendor)
		{
			if( (vendor == probe_matrix[loop].vendor) &&
			    (device == probe_matrix[loop].device) )
			{
				if(probe_matrix[loop].check)
				{
					probe_matrix[loop].check();
					return 1;
				} else {
					printk(KERN_WARNING "ECC: Unsupported device %x:%x.\n", vendor, device);
					return 0;
				}
			}
			loop++;
		}
		printk(KERN_DEBUG "ECC: Unknown device %x:%x.\n", vendor, device);
	}
	printk(KERN_ERR "ECC: Can't find host bridge.\n");
	return 0;
}

static void ecc_fini(void) {
        unload = 1;
        wmb();
        del_timer_sync(&ecctimer);
#ifdef CONFIG_PROC_FS
	remove_proc_entry("ram", 0);
#endif
       if (ecc_sysctl_header) {
               unregister_sysctl_table(ecc_sysctl_header);
               ecc_sysctl_header = NULL;
       }

       /* 
        * If we are expecting to find errors in the logs, we need notification
        * when the module is unloaded.
        */
       if (log_sbe || log_mbe || panic_on_mbe) {
               printk("ECC: Unloaded\n");
       }
}

static int ecc_init(void) {
	int loop;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *ent;
#endif
	printk(KERN_INFO "ECC: monitor version %s\n", ECC_VER);

	for (loop=0;loop<MAXBANKS;loop++) {
		bank[loop].endaddr = 0;
		bank[loop].sbecount = 0;
		bank[loop].mbecount = 0;
		bank[loop].eccmode = 0;
		bank[loop].mtype = ECC_RESERVED;
	}

       if (!find_chipset()) {
               printk("ECC: failed to find a supported memory controller\n");
		return -ENODEV;
       }

#ifdef CONFIG_PROC_FS
	ent = create_proc_entry("ram", S_IFREG | S_IRUGO, 0);
	if (ent) {
		ent->nlink = 1;
		ent->read_proc = procfile_read;
	}
#endif
        ecc_sysctl_header = register_sysctl_table(ecc_root_table, 1);

	init_timer(&ecctimer);
	ecctimer.expires = jiffies + (HZ * poll_msec) / 1000;
	ecctimer.function = (void *)&checkecc;
	add_timer(&ecctimer);

	return 0; 
}

#ifdef MODULE
int init_module(void) {
       return ecc_init();
}
void cleanup_module(void) {
       ecc_fini();
}
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif
