SUBDIR=src/computation

all: $(SUBDIR)

$(SUBDIR):
	$(MAKE) -C $@
	cp $(SUBDIR)/nfa_error .

clean:
	rm -f nfa_error
	$(MAKE) -C $(SUBDIR) clean

no-data:
	rm -f *.fa *.dot *.jpg

.PHONY: all clean $(SUBDIR)
