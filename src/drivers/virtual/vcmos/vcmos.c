/* vcmos.c -- Virtual CMOS. */

/*
 * basic emulation - the time and date is read from the actual cmos
 * all other values (basically the hardware setup) are read on a per
 * process basis. NONE of the cmos ram values can be set at the moment
 *
 */

#if 0
  #define DEBUG
#endif

#define ADEBUG

#include <vmm/kernel.h>
#include <vmm/tasks.h>
#include <vmm/vm.h>
#include <vmm/vcmos.h>
#include <vmm/mc146818rtc.h>
#include <vmm/time.h>
#include <vmm/vpic.h>

#define kprintf kernel->printf

struct kernel_module *kernel;
struct vm_module *vm;
struct vpic_module *vpic;

static int vm_slot;

static void recalc_checksum(struct vm *vm);

static const int days_per_month[2][12] = {
  { 31, 28, 31, 31, 30, 30, 31, 31, 30, 31, 30, 31 },
  { 31, 29, 31, 31, 30, 30, 31, 31, 30, 31, 30, 31 }
};

static void
rtc_update(void *vmach)
{
    u_char h, mi, s, dw, dm, mo, y, c;
    struct vcmos *v = ((struct vm *)vmach)->slots[vm_slot];
    u_char *p;
    int l;
    int doirq = 0;
 
    p = v->cmos_mem; 

    /* check if we should update the clock */
    if(p[RTC_CONTROL] & RTC_SET) {
      goto reset_update;
    }
    p[RTC_REG_A] |= RTC_UIP;
    /* update the clock */
    h = p[RTC_HOURS];
    mi = p[RTC_MINUTES];
    s = p[RTC_SECONDS];
    if(!(p[RTC_CONTROL] & RTC_DM_BINARY)) {
        BCD_TO_BIN(h);
        BCD_TO_BIN(s);
        BCD_TO_BIN(mi);
    }
    if(++s % 60) goto tupdate_done;
    s = 0;
    if(++mi % 60) goto tupdate_done;
    mi = 0;
    if(++h % 24) goto tupdate_done;
    h = 0; 

    dw = p[RTC_DAY_OF_WEEK];
    dm = p[RTC_DAY_OF_MONTH];
    mo = p[RTC_MONTH];
    y = p[RTC_YEAR];
    if(!(p[RTC_CONTROL] & RTC_DM_BINARY)) {
        BCD_TO_BIN(dw);
        BCD_TO_BIN(dm);
        BCD_TO_BIN(mo);
        BCD_TO_BIN(y);
    }
    dw++;
    dw %= 7; 
  
    l = ((y % 4) == 0) && (((y % 100) != 0) || ((y % 400) == 0));

    dm++;
    if(dm <= days_per_month[l][mo]) goto dupdate_done;
    mo++;
    dm = 1;
    if(mo <= 12) goto dupdate_done;
    y++;
    mo = 1;
    if(y <= 99) goto dupdate_done;
    y = 0;
    c = p[0x32];
    BCD_TO_BIN(c);
    c++;
    BIN_TO_BCD(c);
    p[0x32] = c;

dupdate_done:
    if(!(p[RTC_CONTROL] & RTC_DM_BINARY)) {
        BIN_TO_BCD(dw);
        BIN_TO_BCD(dm);
        BIN_TO_BCD(mo);
        BIN_TO_BCD(y);
    }
    p[RTC_DAY_OF_WEEK] = dw;
    p[RTC_DAY_OF_MONTH] = dm;
    p[RTC_MONTH] = mo;
    p[RTC_YEAR] = y;
    
tupdate_done:
    if(!(p[RTC_CONTROL] & RTC_DM_BINARY)) {
        BIN_TO_BCD(s);
        BIN_TO_BCD(h);
        BIN_TO_BCD(mi);
    }
    p[RTC_HOURS] = h;
    p[RTC_MINUTES] = mi;
    p[RTC_SECONDS] = s;
    p[RTC_INTR_FLAGS] = 0;
    if(p[RTC_CONTROL] & RTC_UIE) {
        p[RTC_INTR_FLAGS] |= (RTC_IRQF | RTC_UF);
        doirq++;
    }
       /* check alarm */
    if( (p[RTC_CONTROL] & RTC_AIE) ) {
#ifdef ADEBUG
kprintf("checking alarm..");
#endif
        if( (p[RTC_HOURS] == p[RTC_HOURS_ALARM]) && 
        (p[RTC_MINUTES] == p[RTC_MINUTES_ALARM]) &&
        (p[RTC_SECONDS] == p[RTC_SECONDS_ALARM]) ) {

        p[RTC_INTR_FLAGS] |= (RTC_IRQF | RTC_AF);
#ifdef ADEBUG
kprintf("Alarm Interrupt\n");
#endif
        doirq++;
     } else {
#ifdef ADEBUG
kprintf("no Alarm Interrupt\n");
#endif
     }
   }
    
    p[RTC_REG_A] &= ~RTC_UIP;
reset_update:
    kernel->remove_timer(&v->timer);
    set_timer_func(&v->timer, 1024, rtc_update, vmach);
    kernel->add_timer(&v->timer);
#if 1
    if(doirq && (vpic != NULL)) vpic->simulate_irq(vmach, 8);
#endif
}


void
delete_vcmos(struct vm *vm)
{
    struct vcmos *v = vm->slots[vm_slot];
    if(v != NULL)
    {
        kernel->remove_timer(&v->timer);
	vm->slots[vm_slot] = NULL;
	kernel->free(v);
	vcmos_module.base.vxd_base.open_count--;
    }
}

static bool
create_vcmos(struct vm *vmach, int argc, char **argv)
{
    struct vcmos *new = kernel->calloc(sizeof(struct vcmos), 1);
    if(new != NULL)
    {
	vcmos_module.base.vxd_base.open_count++;
	new->kh.func = delete_vcmos;
	vm->add_vm_kill_handler(vmach, &new->kh);
	vmach->slots[vm_slot] = new;
        vmach->hardware.got_cmos = 1;

	new->vm = vmach;
        new->cmos_reg = 0;
	/* now fill in the initial CMOS values */

	/* the clock */
	new->cmos_mem[RTC_SECONDS] = CMOS_READ(RTC_SECONDS);
	new->cmos_mem[RTC_SECONDS_ALARM] = CMOS_READ(RTC_SECONDS_ALARM);
	new->cmos_mem[RTC_MINUTES] = CMOS_READ(RTC_MINUTES);
	new->cmos_mem[RTC_MINUTES_ALARM] = CMOS_READ(RTC_MINUTES_ALARM);
	new->cmos_mem[RTC_HOURS] = CMOS_READ(RTC_HOURS);
	new->cmos_mem[RTC_HOURS_ALARM] = CMOS_READ(RTC_HOURS_ALARM);
	new->cmos_mem[RTC_DAY_OF_WEEK] = CMOS_READ(RTC_DAY_OF_WEEK);
	new->cmos_mem[RTC_DAY_OF_MONTH] = CMOS_READ(RTC_DAY_OF_MONTH);
	new->cmos_mem[RTC_MONTH] = CMOS_READ(RTC_MONTH);
	new->cmos_mem[RTC_YEAR] = CMOS_READ(RTC_YEAR);

	new->cmos_mem[RTC_REG_A] = CMOS_READ(RTC_REG_A);
	new->cmos_mem[RTC_REG_B] = CMOS_READ(RTC_REG_B);
	new->cmos_mem[RTC_REG_B] &= ~(RTC_PIE | RTC_AIE | RTC_UIE);
	new->cmos_mem[RTC_REG_C] = 0;
	new->cmos_mem[RTC_REG_D] = RTC_VRT;

	/* The system setup */
	new->cmos_mem[0x0E] = 0;	/* diagnostic - no probs */
	new->cmos_mem[0x0F] = 0;	/* shutdown byte - no error */

#if 0
	/* installed floppy disks */
	new->cmos_mem[0x10] = (vmach->hardware.floppy_types[0] & 0xF) << 4 |
			      (vmach->hardware.floppy_types[1] & 0xF);
#endif

	new->cmos_mem[0x11] = 0;	/* reserved */
#if 0
	/* hard disks types - set to 47 (user type) */
DB(("Total Hard disks: %d\n", vmach->hardware.total_hdisks));
	switch(vmach->hardware.total_hdisks) {
	    case 0:
	    new->cmos_mem[0x12] = 0;
            new->cmos_mem[0x19] = 0;
            new->cmos_mem[0x1A] = 0;
            break;
           
            case 1:
            new->cmos_mem[0x12] = 0xF0;
            new->cmos_mem[0x19] = 47;
            new->cmos_mem[0x1A] = 0;
            break;

            case 2:
            new->cmos_mem[0x12] = 0xFF;
            new->cmos_mem[0x19] = 47;
            new->cmos_mem[0x1A] = 47;
            break;
        }

	/* equipment byte */
	if(vmach->hardware.floppy_types[1]) new->cmos_mem[0x14] |= 0x40;
#endif
	new->cmos_mem[0x13] = 0;	/* reserved */
	new->cmos_mem[0x14] |= (vmach->hardware.monitor_type & 0x03) << 4;
	new->cmos_mem[0x14] |= 0x0D;	/* keyboard, display enabled, floppy
					   installed */
	new->cmos_mem[0x14] |= (vmach->hardware.proc.fpu_type) ? 2 : 0;

	/* physical ram stuff */
	new->cmos_mem[0x15] = vmach->hardware.base_mem & 0xFF;
	new->cmos_mem[0x16] = (vmach->hardware.base_mem >> 8) & 0xFF;
	new->cmos_mem[0x17] = vmach->hardware.extended_mem & 0xFF;
	new->cmos_mem[0x18] = (vmach->hardware.extended_mem >> 8) & 0xFF;

	/* Following 19 values are reserved (custom usage for each bios) */
	new->cmos_mem[0x1B] = 0;
	new->cmos_mem[0x1C] = 0;
	new->cmos_mem[0x1D] = 0;
	new->cmos_mem[0x1E] = 0;
	new->cmos_mem[0x1F] = 0;
	new->cmos_mem[0x20] = 0;
	new->cmos_mem[0x21] = 0;
	new->cmos_mem[0x22] = 0;
	new->cmos_mem[0x23] = 0;
	new->cmos_mem[0x24] = 0;
	new->cmos_mem[0x25] = 0;
	new->cmos_mem[0x26] = 0;
	new->cmos_mem[0x27] = 0;
	new->cmos_mem[0x28] = 0;
	new->cmos_mem[0x29] = 0;
	new->cmos_mem[0x2A] = 0;
	new->cmos_mem[0x2B] = 0;
	new->cmos_mem[0x2C] = 0;
	new->cmos_mem[0x2D] = 0;

	new->cmos_mem[0x30] = 0;
	new->cmos_mem[0x31] = 0;
	new->cmos_mem[0x32] = CMOS_READ(0x32);

	new->cmos_mem[0x33] = 0x80;	/* 128K expansion board fitted */

	/* Following values are reserved (custom usage for each bios) */
	new->cmos_mem[0x34] = 0;	
	new->cmos_mem[0x35] = 0;
	new->cmos_mem[0x36] = 0;
	new->cmos_mem[0x37] = 0;
	new->cmos_mem[0x38] = 0;
	new->cmos_mem[0x39] = 0;
	new->cmos_mem[0x3A] = 0;
	new->cmos_mem[0x3B] = 0;
	new->cmos_mem[0x3C] = 0;
	new->cmos_mem[0x3D] = 0;
	new->cmos_mem[0x3E] = 0;
	new->cmos_mem[0x3F] = 0;
        recalc_checksum(vmach);
        set_timer_func(&new->timer, 1024, rtc_update, vmach);
        kernel->add_timer(&new->timer);
        return TRUE;
    }
    return FALSE;
}


/* Virtualisation Stuff */

static u_long
vcmos_in(struct vm *vm, u_short port, int size)
{
    struct vcmos *v = vm->slots[vm_slot];
    if(v == NULL) return -1;
#ifdef DEBUG
#if 0
    if(v->cmos_reg > RTC_REG_D)
    kprintf("direct cmos read: [%x]=%x\n", v->cmos_reg,
        v->cmos_mem[v->cmos_reg]);
#endif
#endif
#if 0
    if(v->cmos_reg <= RTC_REG_D) {
        return CMOS_READ(v->cmos_reg);
    }
    if(v->cmos_reg == 0x32) return CMOS_READ(0x32);
#endif
    if(v->cmos_reg == RTC_INTR_FLAGS) {
        u_long ret;
        ret = v->cmos_mem[RTC_INTR_FLAGS];
        v->cmos_mem[RTC_INTR_FLAGS] = 0;
        return ret;
    }
    return v->cmos_mem[v->cmos_reg];
}

static void
vcmos_out(struct vm *vm, u_short port, int size, u_long val)
{
    struct vcmos *v = vm->slots[vm_slot];
    if(v == NULL) return;
    if(port == CMOS_REG_PORT) {		/* port 0x70 */
        vm->nmi_sts = (val & NMI_SET_BIT) ? NMI_ENABLED : NMI_DISABLED;
        v->cmos_reg = val & 0x3f;
    }
    else if(port == CMOS_DATA_PORT) {	/* port 0x71 */
        v->cmos_mem[v->cmos_reg] = (u_char)val;
#ifdef DEBUG
        if(v->cmos_reg > RTC_REG_D)
        kprintf("cmos reg set: [%x]=%x\n", v->cmos_reg, (u_char)val);
#endif
    }
    return;
}

static void
recalc_checksum(struct vm *vm)
{
    /* checksum - summation of memory 0x10 to 0x2d */
    u_int16 chksum = 0;
    int i;
    struct vcmos *v = vm->slots[vm_slot];

    for(i = 0x10; i <= 0x2d; i++) chksum += v->cmos_mem[i];
    v->cmos_mem[0x2E] = (chksum >> 8 & 0xFF);
    v->cmos_mem[0x2F] = (chksum & 0xFF);
}


void
set_vcmos_byte(struct vm *vm, u_int addr, u_char val)
{
    struct vcmos *v = vm->slots[vm_slot];
    if(addr < 0x3f)
    {
        v->cmos_mem[addr] = val;
        recalc_checksum(vm);
    }
}

u_char
get_vcmos_byte(struct vm *vm, u_int addr)
{
    struct vcmos *v = vm->slots[vm_slot];
    if(addr < 0x3f)
    {
        return v->cmos_mem[addr];
    }
    return 0;
}

void get_bcd_time(struct vm *vm, struct vm86_regs *regs)
{
    u_char h, m, s;
    struct vcmos *v = vm->slots[vm_slot];
    u_char *p;
#ifdef DEBUG
    kprintf("vcmos: get_bcd_time\n");
#endif
    p = v->cmos_mem;
    h = p[RTC_HOURS];
    m = p[RTC_MINUTES];
    s = p[RTC_SECONDS];
    if(p[RTC_CONTROL] & RTC_DM_BINARY) {
        BIN_TO_BCD(h);
        BIN_TO_BCD(m);
        BIN_TO_BCD(s);
    }
    regs->ecx = SET8H(regs->ecx, h);
    regs->ecx = SET8(regs->ecx, m);
    regs->edx = SET8H(regs->edx, s);
    regs->edx = SET8(regs->edx, (p[RTC_CONTROL] & RTC_DST_EN) ? 0 : 1);
    if(p[RTC_CONTROL] & RTC_SET) {
        STC(regs);
    } else {
        CLC(regs);
    }
}
    
void set_bcd_time(struct vm *vm, struct vm86_regs *regs) 
{
    u_char h, m, s, d;
    struct vcmos *v = vm->slots[vm_slot];
    u_char *p;
#ifdef DEBUG
    kprintf("vcmos: set_bcd_time\n");
#endif
    p = v->cmos_mem;
    h = GET8H(regs->ecx);
    m = GET8(regs->ecx);
    s = GET8H(regs->edx);
    d = GET8(regs->edx);
    if(p[RTC_CONTROL] & RTC_DM_BINARY) {
        BCD_TO_BIN(h);
        BCD_TO_BIN(m);
        BCD_TO_BIN(s);
    }
    p[RTC_HOURS] = h;
    p[RTC_MINUTES] = m;
    p[RTC_SECONDS] = s;
    if(d) p[RTC_CONTROL] |= RTC_DST_EN;
    else p[RTC_CONTROL] &= ~RTC_DST_EN;
} 

void get_bcd_date(struct vm *vm, struct vm86_regs *regs) 
{
    u_char y, m, d;
    struct vcmos *v = vm->slots[vm_slot];
    u_char *p;
#ifdef DEBUG
    kprintf("vcmos: get_bcd_date\n");
#endif
    p = v->cmos_mem;
    y = p[RTC_YEAR];
    m = p[RTC_MONTH];
    d = p[RTC_DAY_OF_MONTH];
    if(p[RTC_CONTROL] & RTC_DM_BINARY) {
        BCD_TO_BIN(y);
        BCD_TO_BIN(m);
        BCD_TO_BIN(d);
    }
    regs->ecx = SET8H(regs->ecx, p[0x32]);
    regs->ecx = SET8(regs->ecx, y);
    regs->edx = SET8H(regs->edx, m);
    regs->edx = SET8(regs->edx, d);
    if(p[RTC_CONTROL] & RTC_SET) {
        STC(regs);
    } else {
        CLC(regs);
    }
} 

void set_bcd_date(struct vm *vm, struct vm86_regs *regs) 
{
    u_char y, m, d;
    struct vcmos *v = vm->slots[vm_slot];
    u_char *p;
#ifdef DEBUG
    kprintf("vcmos: set_bcd_date\n");
#endif
    p = v->cmos_mem;
    p[0x32] = GET8H(regs->ecx);
    y = GET8(regs->ecx);
    m = GET8H(regs->edx);
    d = GET8(regs->edx);
    if(p[RTC_CONTROL] & RTC_DM_BINARY) {
        BCD_TO_BIN(y);
        BCD_TO_BIN(m);
        BCD_TO_BIN(d);
    }
    p[RTC_YEAR] = y;
    p[RTC_MONTH] = m;
    p[RTC_DAY_OF_MONTH] = d;
} 

void set_alarm(struct vm *vm, struct vm86_regs *regs)
{
    u_char h, m, s;
    struct vcmos *v = vm->slots[vm_slot];
    u_char *p;
#ifdef DEBUG
    kprintf("vcmos: set_alarm\n");
#endif
    p = v->cmos_mem;
    if(p[RTC_CONTROL] & (RTC_SET | RTC_AIE)) {
        STC(regs);
        return;
    }
    h = GET8H(regs->ecx);
    m = GET8(regs->ecx);
    s = GET8H(regs->edx);
    if(p[RTC_CONTROL] & RTC_DM_BINARY) {
        BCD_TO_BIN(h);
        BCD_TO_BIN(m);
        BCD_TO_BIN(s);
    }
    p[RTC_HOURS_ALARM] = h;
    p[RTC_MINUTES_ALARM] = m;
    p[RTC_SECONDS_ALARM] = s;
    p[RTC_CONTROL] |= RTC_AIE;
    if(vpic != NULL) {
        vpic->set_mask(vm, FALSE, 1<<8);
    }
    CLC(regs);
} 

void reset_alarm(struct vm *vm, struct vm86_regs *regs)
{
    struct vcmos *v = vm->slots[vm_slot];
    u_char *p;
#ifdef DEBUG
    kprintf("vcmos: reset_alarm\n");
#endif
    p = v->cmos_mem;
    p[RTC_CONTROL] &= ~RTC_AIE;
} 



/* Module stuff. */

static struct io_handler cmos_io = {
    NULL, "cmos", CMOS_ADDR_LOW, CMOS_ADDR_HI, vcmos_in, vcmos_out
};

static bool
vcmos_init(void)
{
    vm = (struct vm_module *)kernel->open_module("vm", SYS_VER);
    if(vm != NULL)
    {
        vpic = (struct vpic_module *)kernel->open_module("vpic", SYS_VER);
	vm_slot = vm->alloc_vm_slot();
	if(vm_slot >= 0)
	{
            vm->add_io_handler(NULL, &cmos_io);
	    return TRUE;
	}
	kernel->close_module((struct module *)vm);
    }
    return FALSE;
}

static bool
vcmos_expunge(void)
{
    if(vcmos_module.base.vxd_base.open_count == 0)
    {
        vm->remove_io_handler(NULL, &cmos_io);
	vm->free_vm_slot(vm_slot);
	kernel->close_module((struct module *)vm);
	return TRUE;
    }
    return FALSE;
}

struct vcmos_module vcmos_module =
{
    { MODULE_INIT("vcmos", SYS_VER, vcmos_init, NULL, NULL, vcmos_expunge),
      create_vcmos },
    delete_vcmos,
    get_vcmos_byte,
    set_vcmos_byte,
    get_bcd_time,
    set_bcd_time,
    get_bcd_date,
    set_bcd_date,
    set_alarm,
    reset_alarm
};
