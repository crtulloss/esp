/* Copyright 2017 Columbia University, SLD Group */

#ifndef __<ACCELERATOR_NAME>_DIRECTIVES_HPP__
#define __<ACCELERATOR_NAME>_DIRECTIVES_HPP__

#if defined(HLS_DIRECTIVES_BASIC)
#define HLS_LOAD_INPUT_BATCH_LOOP		\
	HLS_UNROLL_LOOP(OFF)
#define HLS_LOAD_INPUT_LOOP			\
	HLS_UNROLL_LOOP(OFF)
#define HLS_LOAD_INPUT_PLM_WRITE					\
	HLS_CONSTRAIN_LATENCY(0, 1, "constraint-load-mem-access")

#define HLS_STORE_OUTPUT_BATCH_LOOP		\
	HLS_UNROLL_LOOP(OFF)
#define HLS_STORE_OUTPUT_LOOP			\
	HLS_UNROLL_LOOP(OFF)
#define HLS_STORE_OUTPUT_PLM_READ					\
	HLS_CONSTRAIN_LATENCY(0, 1, "constraint-store-mem-access")
#else
#define HLS_LOAD_INPUT_BATCH_LOOP
#define HLS_LOAD_INPUT_LOOP
#define HLS_LOAD_INPUT_PLM_WRITE

#define HLS_STORE_OUTPUT_BATCH_LOOP
#define HLS_STORE_OUTPUT_LOOP
#define HLS_STORE_OUTPUT_PLM_READ
#endif

#endif /* __<ACCELERATOR_NAME>_DIRECTIVES_HPP_ */