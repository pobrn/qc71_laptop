// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_UTIL_H
#define QC71_UTIL_H

/*
 * linearly transforms the number line so that @in_a and @in_b fall onto
 * @out_a and @out_b, respectively; and returns the position of @v on this
 * transformed number line
 *
 * e.g. linear_mapping(50, 100, 75, 0, 10) = 5
 *
 * @v need not be in [@in_a; @in_b]
 */
#define linear_mapping(in_a, in_b, v, out_a, out_b) (((out_a) * ((in_b) - (v)) + (out_b) * ((v) - (in_a))) / ((in_b) - (in_a)))

#define SET_BIT(value, bit, on) ((on) ? ((value) | (bit)) : ((value) & ~(bit)))

#endif /* QC71_UTIL_H */
