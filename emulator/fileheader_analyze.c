#if 0
void fileheaderanalyze(WORD48 *hdr) {
        int i;
        if (trace) return;
        fprintf(trace, "\tFH[00]=%016llo RECLEN=%llu BLKLEN=%llu RECSPERBLK=%llu SEGSPERBLK=%llu\n",
                hdr[0], (hdr[0]>>33)&077777, (hdr[0]>>18)&077777, (hdr[0]>>6)&07777, hdr[0]&077);
        fprintf(trace, "\tFH[01]=%016llo DATE=%llu TIME=%llu\n",
                hdr[1], (hdr[1]>>24)&0777777, hdr[1]&037777777);
        fprintf(trace, "\tFH[07]=%016llo RECORDS=%llu\n",
                hdr[7], hdr[7]);
        fprintf(trace, "\tFH[08]=%016llo SEGSPERROW=%llu\n",
                hdr[8], hdr[8]);
        fprintf(trace, "\tFH[09]=%016llo MAXROWS=%llu\n",
                hdr[9], hdr[9]&037);
        for (i=10; i<29; i++) {
                if (hdr[i] > 0) {
                        fprintf(trace, "\tFH[%02d]=%016llo DFA=%llu\n", i, hdr[i], hdr[i]);
                }
        }
}
#endif


