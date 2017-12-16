SUBDIR=src

all: $(SUBDIR)

$(SUBDIR):
	$(MAKE) -C $@
	cp $(SUBDIR)/nfa_handler .

clean:
	rm -f nfa_error
	$(MAKE) -C $(SUBDIR) clean

no-data:
	rm -f *.fa *.dot *.jpg *.json *.jsn tmp*

.PHONY: all clean $(SUBDIR)
