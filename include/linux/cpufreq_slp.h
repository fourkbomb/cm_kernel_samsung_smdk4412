
#ifndef _LINUX_CPUFREQ_SLP_H
#define _LINUX_CPUFREQ_SLP_H

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
enum {
	PPMU_LAST_CPU_LOAD,
	PPMU_CPU_LOAD_SLOPE,
	PPMU_LAST_DMC0_LOAD,
	PPMU_LAST_DMC1_LOAD,
	PPMU_CURR_BUS_FREQ,
	BUS_LOCKED_FREQ,
	PPMU_BUS_FREQ_CAUSE,
};

enum {
	CAUSE_IS_CPU_LOAD = 1,
	CAUSE_IS_BUS_LOAD,
	CAUSE_IS_BOTH_CPU_AND_BUS_LOAD,
	CAUSE_IS_BUS_LOCK,
};

struct bus_freq_debug_tag {
	unsigned int ppmu_last_cpu_load;
	unsigned int ppmu_cpu_load_slope;
	unsigned int ppmu_last_dmc0_load;
	unsigned int ppmu_last_dmc1_load;
	unsigned int ppmu_curr_bus_freq;
	unsigned int bus_locked_freq;
	unsigned int ppmu_bus_freq_cause;
};

extern struct bus_freq_debug_tag bus_freq_debug;

void store_bus_freq_debug(int type, unsigned int data);
#endif


#if defined(CONFIG_SLP_MINI_TRACER)
void kernel_mini_tracer(bool time_stamp_on, char *input_string);
void kernel_mini_tracer_smp(char *input_string);
#endif

void cpu_load_touch_event(unsigned int event);

#if defined(CONFIG_SLP_CHECK_CPU_LOAD)
void __slp_store_task_history(unsigned int cpu, struct task_struct *task);

static inline void slp_store_task_history(unsigned int cpu
					, struct task_struct *task)
{
	__slp_store_task_history(cpu, task);
}
#else
static inline void slp_store_task_history(unsigned int cpu
					, struct task_struct *task)
{

}
#endif


#endif
