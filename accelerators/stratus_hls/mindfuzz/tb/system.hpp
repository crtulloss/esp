// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __SYSTEM_HPP__
#define __SYSTEM_HPP__

#include "mindfuzz.hpp"
#include "mindfuzz_directives.hpp"

#include "esp_templates.hpp"

//const size_t MEM_SIZE = 69599232 / (DMA_WIDTH/8);
// changed to reflect 32b data
const size_t MEM_SIZE = 139198464 / (DMA_WIDTH/8);

#include "core/systems/esp_system.hpp"

#ifdef CADENCE
#include "mindfuzz_wrap.h"
#endif

class system_t : public esp_system<DMA_WIDTH, MEM_SIZE>
{
public:

    // ACC instance
#ifdef CADENCE
    mindfuzz_wrapper *acc;
#else
    mindfuzz *acc;
#endif

    // Constructor
    SC_HAS_PROCESS(system_t);
    system_t(sc_module_name name)
        : esp_system<DMA_WIDTH, MEM_SIZE>(name)
    {
        // ACC
#ifdef CADENCE
        acc = new mindfuzz_wrapper("mindfuzz_wrapper");
#else
        acc = new mindfuzz("mindfuzz_wrapper");
#endif
        // Binding ACC
        acc->clk(clk);
        acc->rst(acc_rst);
        acc->dma_read_ctrl(dma_read_ctrl);
        acc->dma_write_ctrl(dma_write_ctrl);
        acc->dma_read_chnl(dma_read_chnl);
        acc->dma_write_chnl(dma_write_chnl);
        acc->conf_info(conf_info);
        acc->conf_done(conf_done);
        acc->acc_done(acc_done);
        acc->debug(debug);

        /* <<--params-default-->> */
        do_relu = 0;
        window_size = 4;
        batches_perload = 1;
        neurons_perwin = 1;
        tsamps_perbatch = 70;
        detect_threshold = 100.0;
        num_windows = 7;
        iters_perbatch = 1;
        num_loads = 500;
        learning_rate = ((TYPE)0.000001) / ((TYPE)tsamps_perbatch) / ((TYPE)window_size);

    }

    // Processes

    // Configure accelerator
    void config_proc();

    // Load internal memory
    void load_memory();

    // Dump internal memory
    void dump_memory();

    // Validate accelerator results
    int validate();

    // Accelerator-specific data
    /* <<--params-->> */
    int32_t do_relu;
    int32_t window_size;
    int32_t batches_perload;
    TYPE learning_rate;
    int32_t neurons_perwin;
    int32_t tsamps_perbatch;
    TYPE detect_threshold;
    int32_t num_windows;
    int32_t iters_perbatch;
    int32_t num_loads;

    uint32_t in_words_adj;
    uint32_t out_words_adj;
    uint32_t in_size;
    uint32_t out_size;
    float *in;
    float *out;
    float *gold;

    // Other Functions
};

#endif // __SYSTEM_HPP__