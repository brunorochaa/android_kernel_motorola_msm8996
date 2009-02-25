#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/pfn.h>

#include <asm/e820.h>

static u64 patterns[] __initdata = {
	0,
	0xffffffffffffffffULL,
	0x5555555555555555ULL,
	0xaaaaaaaaaaaaaaaaULL,
};

static void __init reserve_bad_mem(u64 pattern, unsigned long start_bad,
			    unsigned long end_bad)
{
	printk(KERN_CONT "\n  %016llx bad mem addr "
	       "%010lx - %010lx reserved",
	       (unsigned long long) pattern, start_bad, end_bad);
	reserve_early(start_bad, end_bad, "BAD RAM");
}

static void __init memtest(unsigned long start_phys, unsigned long size,
			   u64 pattern)
{
	unsigned long i;
	u64 *start;
	unsigned long start_bad;
	unsigned long last_bad;
	unsigned long start_phys_aligned;
	unsigned long count;
	unsigned long incr;

	incr = sizeof(pattern);
	start_phys_aligned = ALIGN(start_phys, incr);
	count = (size - (start_phys_aligned - start_phys))/incr;
	start = __va(start_phys_aligned);
	start_bad = 0;
	last_bad = 0;

	for (i = 0; i < count; i++)
		start[i] = pattern;
	for (i = 0; i < count; i++, start++, start_phys_aligned += incr) {
		if (*start == pattern)
			continue;
		if (start_phys_aligned == last_bad + incr) {
			last_bad += incr;
			continue;
		}
		if (start_bad)
			reserve_bad_mem(pattern, start_bad, last_bad + incr);
		start_bad = last_bad = start_phys_aligned;
	}
	if (start_bad)
		reserve_bad_mem(pattern, start_bad, last_bad + incr);
}

/* default is disabled */
static int memtest_pattern __initdata;

static int __init parse_memtest(char *arg)
{
	if (arg)
		memtest_pattern = simple_strtoul(arg, NULL, 0);
	return 0;
}

early_param("memtest", parse_memtest);

void __init early_memtest(unsigned long start, unsigned long end)
{
	u64 t_start, t_size;
	unsigned int i;
	u64 pattern;

	if (!memtest_pattern)
		return;

	printk(KERN_INFO "early_memtest: pattern num %d", memtest_pattern);
	for (i = 0; i < memtest_pattern; i++) {
		unsigned int idx = i % ARRAY_SIZE(patterns);
		pattern = patterns[idx];
		t_start = start;
		t_size = 0;
		while (t_start < end) {
			t_start = find_e820_area_size(t_start, &t_size, 1);

			/* done ? */
			if (t_start >= end)
				break;
			if (t_start + t_size > end)
				t_size = end - t_start;

			printk(KERN_CONT "\n  %010llx - %010llx pattern %d",
			       (unsigned long long)t_start,
			       (unsigned long long)t_start + t_size, idx);

			memtest(t_start, t_size, pattern);

			t_start += t_size;
		}
	}
	printk(KERN_CONT "\n");
}
