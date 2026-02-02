#pragma once

/* Mejiro chord bit positions (0..23)
 * Bit order:  S,T,K,N,Y,I,A,U,n,t,k,# | S,T,K,N,Y,I,A,U,n,t,k,*
 *
 * Note:
 *  - Use lowercase l/r prefixes to avoid clashing with ZMK's LS(x) macro.
 *  - Example keymap: &mj lS
 */

/* left 0..11 */
#define fS  0
#define fT  1
#define fK  2
#define fN  3
#define fY  4
#define fI  5
#define fA  6
#define fU  7
#define fn  8   /* n */
#define ft  9
#define fk 10
#define fX 11   /* # */

/* right 12..23 */
#define jS 12
#define jT 13
#define jK 14
#define jN 15
#define jY 16
#define jI 17
#define jA 18
#define jU 19
#define jn 20   /* n */
#define jt 21
#define jk 22
#define jX 23   /* * */
