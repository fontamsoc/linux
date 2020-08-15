// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef HWDRVFBDEV_H
#define HWDRVFBDEV_H

#define HWDRVFBDEV_GETINFO_WIDTH  ((unsigned long)0)
#define HWDRVFBDEV_GETINFO_HEIGHT ((unsigned long)1)
#define HWDRVFBDEV_GETINFO_HZ     ((unsigned long)2)
#define HWDRVFBDEV_GETINFO_BUFCNT ((unsigned long)3)
#define HWDRVFBDEV_GETINFO_ACCELV ((unsigned long)4)

static inline unsigned long hwdrvfbdev_srcset (void *addr, unsigned long vsrc) {
	vsrc <<= 2;
	__asm__ __volatile__ ("ldst %0, %1" : "+r" (vsrc) : "r" (addr));
	return vsrc;
}

static inline unsigned long hwdrvfbdev_getinfo (void *addr, unsigned long param) {
	param <<= 2;
	param |= 1;
	__asm__ __volatile__ ("ldst %0, %1" : "+r" (param) : "r" (addr));
	return param;
}

inline uint32_t* hwdrvfbdev_encblit (uint32_t *dst, uint32_t *src, unsigned long cnt) {
	uint32_t px_hold;
	unsigned long px_hold_cnt = 0;
	while (cnt) {
		uint32_t px_saved = (*src & 0xffffff);
		if (!px_hold_cnt)
			px_hold = px_saved;
		unsigned long px_saved_diff_hold = (px_saved != px_hold);
		if (!px_saved_diff_hold)
			++px_hold_cnt;
		if (px_saved_diff_hold || cnt == 1 || px_hold_cnt == 0xff) {
			*dst = (((px_hold_cnt-2)<<24) | px_hold);
			px_hold_cnt = 0;
			++dst;
		}
		if (!px_saved_diff_hold) {
			++src;
			--cnt;
		}
	}
	return dst;
}

inline uint32_t* hwdrvfbdev_encfill (uint32_t *dst, uint32_t pxval, unsigned long cnt) {
	while (cnt) {
		unsigned long _cnt;
		if (cnt > 0xff) {
			_cnt = (0xff-2);
			cnt -= 0xff;
		} else {
			_cnt = (cnt-2);
			cnt = 0;
		}
		*dst = ((_cnt<<24) | (pxval & 0xffffff));
		++dst;
	}
	return dst;
}

inline uint32_t* hwdrvfbdev_dec (uint32_t *dst, uint32_t *src, unsigned long cnt) {
	while (cnt) {
		uint8_t px_repeat = (((*src >> 24) + 2) & 0xff);
		uint32_t px_val = (*src & 0xffffff);
		if (px_repeat) {
			do {
				--px_repeat;
				*dst = ((0xff << 24) | px_val);
				++dst;
				--cnt;
			} while (px_repeat);
			++src;
		} else {
			// TODO: Reserved for pixel repeat count == 0xFE which will never
			// TODO: be encoded by hwdrvfbdev_encblit() and hwdrvfbdev_encfill().
		}
	}
	return dst;
}

#endif /* HWDRVFBDEV_H */
