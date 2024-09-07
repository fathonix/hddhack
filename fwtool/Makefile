CFLAGS = -ggdb -DSG_IO
OBJS = main.o wdromfmt.o sgio.o diskio.o

fwtool: $(OBJS)
	$(CC) $(LDFLAGS) -o fwtool $(OBJS)

clean:
	rm -f $(OBJS) fwtool
