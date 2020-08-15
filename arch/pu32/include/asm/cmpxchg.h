// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_CMPXCHG_H
#define __ASM_PU32_CMPXCHG_H

#define arch_xchg(p, n)					\
({							\
	__typeof__(p) _p_ = (p);			\
	__typeof__(n) _n_ = (n);			\
	switch (sizeof(*(p))) {				\
		case 1:					\
			__asm__ __volatile__ (		\
				"ldst8 %0, %1"		\
				: "+r" (_n_)		\
				: "r" (_p_));		\
			break;				\
		case 2:					\
			__asm__ __volatile__ (		\
				"ldst16 %0, %1"		\
				: "+r" (_n_)		\
				: "r" (_p_));		\
			break;				\
		case 4:					\
			__asm__ __volatile__ (		\
				"ldst32 %0, %1"		\
				: "+r" (_n_)		\
				: "r" (_p_));		\
			break;				\
		default:				\
			while(1)			\
				*(void **)0 = 0;	\
	}						\
	_n_;						\
})

#define arch_cmpxchg(p, o, n)					\
({								\
	__typeof__(p) _p_ = (p);				\
	__typeof__(n) _n_ = (n);				\
	__typeof__(o) _o_ = (o);				\
	switch (sizeof(*(p))) {					\
		case 1:						\
			__asm__ __volatile__ (			\
				"cpy %%sr, %2; cldst8 %0, %1"	\
				: "+r" (_n_)			\
				: "r" (_p_),			\
				  "r" (_o_)			\
				: "%sr");			\
			break;					\
		case 2:						\
			__asm__ __volatile__ (			\
				"cpy %%sr, %2; cldst16 %0, %1"	\
				: "+r" (_n_)			\
				: "r" (_p_),			\
				  "r" (_o_)			\
				: "%sr");			\
			break;					\
		case 4:						\
			__asm__ __volatile__ (			\
				"cpy %%sr, %2; cldst32 %0, %1"	\
				: "+r" (_n_)			\
				: "r" (_p_),			\
				  "r" (_o_)			\
				: "%sr");			\
			break;					\
		default:					\
			while(1)				\
				*(void **)0 = 0;		\
	}							\
	_n_;							\
})

#endif /* __ASM_PU32_CMPXCHG_H */
