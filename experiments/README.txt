Folder contains the following csv files:

reduction.csv
    Data about reduced NFAs.

    headers:
        automaton - automaton filename
        pcap - train pcap file
        ratio - reduction ratio
        th - merging threshold
        fm - max frequency (merging parameter)
        states - number of states
        trans - number of transitions

eval.csv
    Data about reduced NFAs error.

    automaton - automaton filename
    pcap - test pcap file
    total - total packets with payload
    afp - false positives (acceptance)
    atp - true positives (acceptance)
    cfp - false positives (classification)
    ctp - true positives (classification)

min_stats.csv
    Precise reduction stats applied after approximate reduction.

    nfa - nfa name
    states - number of states
    fin - number of final states
    trans - number of transitions
