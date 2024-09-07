/*
	WD flash firmware hack, copyright Sprite_tm 2013.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdint.h>

#define UART_REG_DATA 2
#define UART_REG_STATUS 6
//volatile uint16_t * const uartRegs=0x1c00a620;

//Ahrg... I'm trying to compile this thing with a linux toolchain and
//the division functions in its libgcc will raise() if something goes wrong...
//so we provide that here to make xprintf work.
void raise() {
}


void uart_putchar(unsigned char c) {
	volatile uint16_t * volatile const uartRegs=0x1c00a620;
	while((uartRegs[UART_REG_STATUS]&(1<<6))==0) ;
	uartRegs[UART_REG_DATA]=c;
}

void xfunc_out(unsigned char c) {
	uart_putchar(c);
}


unsigned char uart_getchar() {

}