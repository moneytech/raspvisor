#include "mm.h"
#include "sched.h"
#include "task.h"
#include "utils.h"
#include "user.h"
#include "printf.h"
#include "entry.h"


static struct pt_regs * task_pt_regs(struct task_struct *tsk)
{
	unsigned long p = (unsigned long)tsk + THREAD_SIZE - sizeof(struct pt_regs);
	return (struct pt_regs *)p;
}

static int prepare_el1_switching(unsigned long start, unsigned long size, unsigned long pc)
{
	struct pt_regs *regs = task_pt_regs(current);
	regs->pstate = PSR_MODE_EL1h;
	regs->pc = pc;
	regs->sp = 2 *  PAGE_SIZE;
	unsigned long code_page = allocate_user_page(current, 0);
	if (code_page == 0)	{
		return -1;
	}
	memcpy(code_page, start, size);
	set_stage2_pgd(current->mm.pgd);
	return 0;
}

static void prepare_vmtask(unsigned long arg){
	printf("vmtask: arg=%d, EL=%d\r\n", arg, get_el());
	unsigned long begin = (unsigned long)&user_begin;
	unsigned long end = (unsigned long)&user_end;
	unsigned long process = (unsigned long)&user_process;
	int err = prepare_el1_switching(begin, end - begin, process - begin);
	if (err < 0){
		printf("Error while moving process to user mode\n\r");
	}

  // switch_from_kthead() will be called and switched to EL1
}

int create_vmtask(unsigned long arg)
{
	preempt_disable();
	struct task_struct *p;

	unsigned long page = allocate_kernel_page();
	p = (struct task_struct *) page;
	struct pt_regs *childregs = task_pt_regs(p);

	if (!p)
		return -1;

  p->cpu_context.x19 = (unsigned long)prepare_vmtask;
  p->cpu_context.x20 = arg;
	p->flags = PF_KTHREAD;
	p->priority = current->priority;
	p->state = TASK_RUNNING;
	p->counter = p->priority;
	p->preempt_count = 1; //disable preemtion until schedule_tail

	p->cpu_context.pc = (unsigned long)switch_from_kthread;
	p->cpu_context.sp = (unsigned long)childregs;
	int pid = nr_tasks++;
	task[pid] = p;

	preempt_enable();
	return pid;
}
