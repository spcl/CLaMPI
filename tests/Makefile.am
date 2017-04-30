AM_CFLAGS=-I../src/ -DCL_HT_ENTRIES_VAL=400 -DCL_MAX_SCAN=10 -DNOFLUSH -DNGETS=20000 -DTOT=10
#-static

bin_PROGRAMS = correctness_single_rank correctness_multiple_ranks

correctness_single_rank_SOURCES = correctness_single_rank.c
correctness_single_rank_LDADD = $(top_builddir)/src/libclampi.la
#correctness_single_rank_DEPENDENCIES = libclampi.la

correctness_multiple_ranks_SOURCES = correctness_multiple_ranks.c
correctness_multiple_ranks_LDADD = $(top_builddir)/src/libclampi.la


if HAVE_LIBLSB
    bin_PROGRAMS += perf_hash
    perf_hash_SOURCES = perf_hash.c
    perf_hash_LDFLAGS = -static
    perf_hash_LDADD = $(top_builddir)/src/libclampi.la -llsb
endif


