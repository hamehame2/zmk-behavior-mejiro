#pragma once

/* Mejiro chord bit positions (0..23)
 * Bit order:  S,T,K,N,Y,I,A,U,n,t,k,# | S,T,K,N,Y,I,A,U,n,t,k,*
 *
 * Note:
 *  - Use lowercase l/r prefixes to avoid clashing with ZMK's LS(x) macro.
 *  - Example keymap: &mj lS
 */

/* left 0..11 */
#define lS  0
#define lT  1
#define lK  2
#define lN  3
#define lY  4
#define lI  5
#define lA  6
#define lU  7
#define ln  8   /* n */
#define lt  9
#define lk 10
#define lX 11   /* # */

/* right 12..23 */
#define rS 12
#define rT 13
#define rK 14
#define rN 15
#define rY 16
#define rI 17
#define rA 18
#define rU 19
#define rn 20   /* n */
#define rt 21
#define rk 22
#define rX 23   /* * */
